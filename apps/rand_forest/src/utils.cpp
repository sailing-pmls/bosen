// Author: Jiesi Zhao (jiesizhao0423@gmail.com), Wei Dai (wdai@cs.cmu.edu)
// Date: 2014.11.5

#include "utils.hpp"
#include <math.h>
#include <vector>
#include <glog/logging.h>
#include <ml/include/ml.hpp>

namespace tree {

// Compute Entropy of distribution (assuming \sum_i dist[i] = 1).
float ComputeEntropy(const std::vector<float> &dist) {
  float entropy = 0.0;
  for (auto iter = dist.begin(); iter != dist.end(); ++iter) {
    if (*iter) {
      entropy -= (*iter) * fastlog2(*iter);
    }
  }
  if (entropy < 1e-5) {
    entropy = 0.0;
  }
  return entropy;
}

// Normalize
void Normalize(std::vector<float>* count) {
  float sum = 0.;
  for (auto iter = count->begin(); iter != count->end(); ++iter) {
    sum += (*iter);
  }
  for (auto iter = count->begin(); iter != count->end(); ++iter) {
    (*iter) /= sum;
  }
}

}  // namespace tree


namespace {
	void gen_binary_label(const std::vector<float>& y_pred, std::vector<int>& y_pred_label, float threshold = 0.5) {
		int size = y_pred.size();
		y_pred_label.reserve(size);
		for (int i = 0; i < size; i++) {
			y_pred_label[i] = y_pred[i] >= threshold ? 1 : 0;
		}
	}
	void gen_multi_label(const std::vector<std::vector<float> >& y_pred, std::vector<int>& y_pred_label, int n_classes) {
		int size = y_pred.size();
		float max_proba;
		y_pred_label.reserve(size);

		for (int i = 0; i < size; i++) {
			max_proba = 0.0;	
			for (int k = 0; k < n_classes; k++) {
				if (y_pred[i][k] > max_proba) {
					max_proba = y_pred[i][k];
					y_pred_label[i] = k;
				}
			}
		}
	}

	float precision_bin(const std::vector<int>& y_pred, const std::vector<int>& y_true) {
		int size = y_pred.size(), TP, FP, val;
		CHECK_EQ(size, y_true.size());

		TP = FP = 0;
		for (int i = 0; i < size; i++) {
			val = (y_pred[i] << 1) + y_true[i];
			if (val == 3) TP++;
			else if (val == 2) FP++;
		}
		if (TP+FP == 0) 
			return 0;
		else 
			return (float)TP / (TP+FP);
	}
	float precision_bin(const std::vector<float>& y_pred, const std::vector<int>& y_true, float threshold = 0.5) {
		std::vector<int> y_pred_label;	
		gen_binary_label(y_pred, y_pred_label, threshold);

		return precision_bin(y_pred_label, y_true);
	}
	float precision_mul(const std::vector<int>& y_pred, const std::vector<int>& y_true, int n_classes) {
		int size = y_pred.size();
		float avg_precision;
		CHECK_EQ(size, y_true.size());
		std::vector<int> true_positive, positive;
		true_positive.reserve(n_classes);
		positive.reserve(n_classes);

		for (int i = 0; i < size; i++) {
			if (y_pred[i] == y_true[i]) {
				true_positive[y_true[i]]++;
			}
			positive[y_pred[i]]++;
		}
			
		avg_precision = 0.0;	
		for (int c = 0; c < n_classes; c++) {
			avg_precision += (float) true_positive[c] / positive[c];
		}
		avg_precision /= n_classes;

		return avg_precision;
	}
	float precision_mul(const std::vector<std::vector<float> >& y_pred, const std::vector<int>& y_true, int n_classes) {
		std::vector<int> y_pred_label;
		gen_multi_label(y_pred, y_pred_label, n_classes);

		return precision_mul(y_pred_label, y_true, n_classes);
	}

	float recall_bin(const std::vector<int>& y_pred, const std::vector<int>& y_true) {
		int size = y_pred.size(), TP, FN, val;
		CHECK_EQ(size, y_true.size());

		TP = FN = 0;
		for (int i = 0; i < size; i++) {
			val = (y_pred[i] << 1) + y_true[i];
			if (val == 3) TP++;
			else if (val == 1) FN++;
		}
		if (TP + FN == 0) 
			return 0.0;
		else 
			return (float)TP / (TP+FN);
	}
	float recall_bin(const std::vector<float>& y_pred, const std::vector<int>& y_true, float threshold = 0.5) {
		std::vector<int> y_pred_label;
		gen_binary_label(y_pred, y_pred_label, threshold);
		
		return recall_bin(y_pred_label, y_true);
	}
	float recall_mul(const std::vector<int>& y_pred, const std::vector<int>& y_true, int n_classes) {
		int size = y_pred.size();
		float avg_recall;
		CHECK_EQ(size, y_true.size());
		std::vector<int> true_positive, true_label;
		true_positive.reserve(n_classes);
		true_label.reserve(n_classes);

		for (int i = 0; i < size; i++) {
			if (y_pred[i] == y_true[i]) {
				true_positive[y_true[i]]++;
			}
			true_label[y_true[i]]++;
		}

		avg_recall = 0.0;
		for (int c = 0; c < n_classes; c++) {
			avg_recall += (float) true_positive[c] / true_label[c];
		}
		avg_recall /= n_classes;

		return avg_recall;
	}
	float recall_mul(const std::vector<std::vector<float> >& y_pred, const std::vector<int>& y_true, int n_classes) {
		std::vector<int> y_pred_label;
		gen_multi_label(y_pred, y_pred_label, n_classes);

		return recall_mul(y_pred_label, y_true, n_classes);
	}

	float f1_score_bin(const std::vector<float>& y_pred, const std::vector<int>& y_true, float threshold = 0.5) {
		float precision_val, recall_val;
		precision_val = precision_bin(y_pred, y_true, threshold);
		recall_val = recall_bin(y_pred, y_true, threshold);

		return 2*precision_val*recall_val / (precision_val+recall_val);
	}
	float f1_score_bin(const std::vector<int>& y_pred, const std::vector<int>& y_true) {
		float precision_val, recall_val;
		precision_val = precision_bin(y_pred, y_true);
		recall_val = recall_bin(y_pred, y_true);

		return 2*precision_val*recall_val / (precision_val+recall_val);
	}
	float f1_score_mul(const std::vector<std::vector<float> >& y_pred, const std::vector<int>& y_true, int n_classes) {
		float precision_val, recall_val;
		precision_val = precision_mul(y_pred, y_true, n_classes);
		recall_val = recall_mul(y_pred, y_true, n_classes);

		return 2*precision_val*recall_val / (precision_val+recall_val);
	}
	float f1_score_mul(const std::vector<int>& y_pred, const std::vector<int>& y_true, int n_classes) {
		float precision_val, recall_val;
		precision_val = precision_mul(y_pred, y_true, n_classes);
		recall_val = recall_mul(y_pred, y_true, n_classes);

		return 2*precision_val*recall_val / (precision_val+recall_val);
	}

	float roc_auc_score(const std::vector<float>& y_pred, const std::vector<int>& y_true) {
		std::vector<int> idx;		
		int size = y_pred.size(), n_pos, n_neg;
		float ret_auc;

		tree::ArgSort(y_pred, idx, -1);
		
		n_pos = n_neg = 0;
		ret_auc = 0.0;
		for (int i = 0; i < size; i++) {
			if (y_true[idx[i]] == 1) {
				n_pos++;
				ret_auc += size - i;
			} else {
				n_neg++;
			}
		}

		ret_auc = (ret_auc - n_pos*(n_pos+1)/2) / (n_pos*n_neg);

		return ret_auc > 1.0 ? 1.0 : ret_auc;
	}
}

namespace tree {
	void getRow(const std::vector<std::vector<float> >& matrix, std::vector<float>& row, int rowId) {
		CHECK(rowId >= 0 && rowId < matrix.size());
		row = matrix[rowId];
	}

	void getCol(const std::vector<std::vector<float> >& matrix, std::vector<float>& col, int colId) {
		int colSize = matrix[0].size(), rowCount = 0;
		CHECK(colId >= 0 && colId < colSize);
		col.reserve(colSize);
		for (auto& row : matrix) {
			CHECK_EQ(row.size(), colSize);
			col[rowCount++];
		}
	}

	float precision(const std::vector<std::vector<float> >& y_pred, const std::vector<int>& y_true, int n_classes, float threshold) {
		CHECK_GE(n_classes, 2);
		if (n_classes == 2) {
			std::vector<float> y_pred_pos;
			getCol(y_pred, y_pred_pos, 1);
			return precision_bin(y_pred_pos, y_true, threshold);
		} else {
			return precision_mul(y_pred, y_true, n_classes);
		}
	}
	float precision(const std::vector<int>& y_pred, const std::vector<int>& y_true, int n_classes) {
		CHECK_GE(n_classes, 2);
		if (n_classes == 2) {
			return precision_bin(y_pred, y_true);
		} else {
			return precision_mul(y_pred, y_true, n_classes);
		}
	}

	float recall(const std::vector<std::vector<float> >& y_pred, const std::vector<int>& y_true, int n_classes, float threshold) {
		CHECK_GE(n_classes, 2);
		if (n_classes == 2) {
			std::vector<float> y_pred_pos;
			getCol(y_pred, y_pred_pos, 1);
			return recall_bin(y_pred_pos, y_true, threshold);
		} else {
			return recall_mul(y_pred, y_true, n_classes);
		}
	}
	float recall(const std::vector<int>& y_pred, const std::vector<int>& y_true, int n_classes) {
		CHECK_GE(n_classes, 2);
		if (n_classes == 2) {
			return recall_bin(y_pred, y_true);
		} else {
			return recall_mul(y_pred, y_true, n_classes);
		}
	}

	float f1_score(const std::vector<std::vector<float> >& y_pred, const std::vector<int>& y_true, int n_classes, float threshold) {
		CHECK_GE(n_classes, 2);
		if (n_classes == 2) {
			std::vector<float> y_pred_pos;
			getCol(y_pred, y_pred_pos, 1);
			return f1_score_bin(y_pred_pos, y_true, threshold);
		} else {
			return f1_score_mul(y_pred, y_true, n_classes);
		}
	}
	float f1_score(const std::vector<int>& y_pred, const std::vector<int>& y_true, int n_classes) {
		CHECK_GE(n_classes, 2);
		if (n_classes == 2) {
			return f1_score_bin(y_pred, y_true);
		} else {
			return f1_score_mul(y_pred, y_true, n_classes);
		}
	}
};
