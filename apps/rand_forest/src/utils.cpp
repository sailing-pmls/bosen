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

void Int2Float(const std::vector<int>& v1, std::vector<float>& v2) {
	v2.resize(v1.size());
	for (int i = 0; i < v1.size(); i++) {
		v2[i] = (float)v1[i];
	}
}

}  // namespace tree


namespace {
	void gen_binary_label(const std::vector<float>& y_pred, std::vector<int>& y_pred_label, float threshold = 0.5) {
		int size = y_pred.size();
		y_pred_label.resize(size);
		for (int i = 0; i < size; i++) {
			y_pred_label[i] = y_pred[i] >= threshold ? 1 : 0;
		}
	}
	void gen_multi_label(const std::vector<std::vector<float> >& y_pred, std::vector<int>& y_pred_label, int n_classes) {
		int size = y_pred.size();
		float max_proba;
		y_pred_label.resize(size);

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
		true_positive.resize(n_classes);
		positive.resize(n_classes);

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
		true_positive.resize(n_classes);
		true_label.resize(n_classes);

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

	float roc_auc_score_multi(const std::vector<std::vector<float> >& y_pred, const std::vector<int>& y_true, int n_classes) {
		float avg_roc_auc_score = 0.0;
		std::vector<int> y_true_oneVSrest;
		std::vector<float> y_pred_oneVSrest;

		CHECK_EQ(y_pred.size(), y_true.size()) << "`y_pred` size do not match with `y_true`";

		y_true_oneVSrest.resize(y_true.size());
		for (int c = 0; c < n_classes; c++) {
			tree::GetCol(y_pred, y_pred_oneVSrest, c);	
			for (int i = 0; i < y_pred.size(); i++) {
				y_true_oneVSrest[i] = y_true[i] == c ? 1 : 0;
			}
			avg_roc_auc_score += roc_auc_score(y_pred_oneVSrest, y_true_oneVSrest);
		}
		avg_roc_auc_score /= n_classes;

		return avg_roc_auc_score;
	}

	float auc(const std::vector<float>& x, std::vector<float>& y) {
		CHECK_EQ(x.size(), y.size()) << "size of vector `x` must match the size of vector `y`";
		std::vector<int> idx;
		float last_x = 0.0, ret_auc = 0.0;

		tree::ArgSort(x, idx, 1);

		for (int i = 0; i < x.size(); i++) {
			CHECK(x[idx[i]] <= 1 && x[idx[i]] >= 0 && y[idx[i]] <= 1 && y[idx[i]] >= 0) << "the 2-d corinates must satisfy 0<=x,y<=1";

			ret_auc += (x[idx[i]] - last_x) * y[idx[i]];

			last_x = x[idx[i]];
		}
		return ret_auc;
	}

	float pr_auc_score(const std::vector<float>& y_pred, const std::vector<int>& y_true) {
		int TP = 0, FP = 0, TN = 0, FN = 0, size;
		std::vector<int> idx;
		std::vector<float> x, y;
		float ret_auc;

		CHECK_EQ(y_pred.size(), y_true.size()) << "`y_pred` size do not match with `y_true`";
		tree::ArgSort(y_pred, idx, -1);

		size = y_pred.size();
		// predict all the examples to negative
		for (int i = 0; i < size; i++) {
			if (y_true[idx[i]] == 1) FN++;
			else TN++;
		}
		
		x.resize(size);
		y.resize(size);

		for (int i = 0; i < size; i++) {
			if (y_true[idx[i]] == 1) {
				FN--;
				TP++;
			} else {
				TN--;
				FP++;
			}
			// recall
			x[i] = (float)TP / (TP+FN);	
			// precision
			y[i] = (float)TP / (TP+FP);
		}
		
		ret_auc = auc(x, y);

		return ret_auc > 1.0 ? 1.0 : ret_auc;
	}
}

float pr_auc_score_multi(const std::vector<std::vector<float> >& y_pred, const std::vector<int>& y_true, int n_classes) {
	float avg_pr_auc_score = 0.0;
	std::vector<int> y_true_oneVSrest;
	std::vector<float> y_pred_oneVSrest;

	CHECK_EQ(y_pred.size(), y_true.size()) << "`y_pred` size do not match with `y_true`";

	y_true_oneVSrest.resize(y_true.size());
	for (int c = 0; c < n_classes; c++) {
		tree::GetCol(y_pred, y_pred_oneVSrest, c);	
		for (int i = 0; i < y_pred.size(); i++) {
			y_true_oneVSrest[i] = y_true[i] == c ? 1 : 0;
		}
		avg_pr_auc_score += pr_auc_score(y_pred_oneVSrest, y_true_oneVSrest);
	}
	avg_pr_auc_score /= n_classes;

	return avg_pr_auc_score;
}

namespace tree {
	void GetRow(const std::vector<std::vector<float> >& matrix, std::vector<float>& row, int rowId) {
		CHECK(rowId >= 0 && rowId < matrix.size());
		row = matrix[rowId];
	}

	void GetCol(const std::vector<std::vector<float> >& matrix, std::vector<float>& col, int colId) {
		int colSize = matrix[0].size(), rowCount = 0;
		CHECK(colId >= 0 && colId < colSize);
		col.resize(matrix.size());
		for (auto& row: matrix) {
			CHECK_EQ(row.size(), colSize);
			col[rowCount++] = row[colId];
		}
	}

	float Precision(const std::vector<std::vector<float> >& y_pred, const std::vector<int>& y_true, int n_classes, float threshold) {
		CHECK_GE(n_classes, 2);
		if (n_classes == 2) {
			std::vector<float> y_pred_pos;
			GetCol(y_pred, y_pred_pos, 1);
			return precision_bin(y_pred_pos, y_true, threshold);
		} else {
			return precision_mul(y_pred, y_true, n_classes);
		}
	}
	float Precision(const std::vector<int>& y_pred, const std::vector<int>& y_true, int n_classes) {
		CHECK_GE(n_classes, 2);
		if (n_classes == 2) {
			return precision_bin(y_pred, y_true);
		} else {
			return precision_mul(y_pred, y_true, n_classes);
		}
	}

	float Recall(const std::vector<std::vector<float> >& y_pred, const std::vector<int>& y_true, int n_classes, float threshold) {
		CHECK_GE(n_classes, 2);
		if (n_classes == 2) {
			std::vector<float> y_pred_pos;
			GetCol(y_pred, y_pred_pos, 1);
			return recall_bin(y_pred_pos, y_true, threshold);
		} else {
			return recall_mul(y_pred, y_true, n_classes);
		}
	}
	float Recall(const std::vector<int>& y_pred, const std::vector<int>& y_true, int n_classes) {
		CHECK_GE(n_classes, 2);
		if (n_classes == 2) {
			return recall_bin(y_pred, y_true);
		} else {
			return recall_mul(y_pred, y_true, n_classes);
		}
	}

	float F1_score(const std::vector<std::vector<float> >& y_pred, const std::vector<int>& y_true, int n_classes, float threshold) {
		CHECK_GE(n_classes, 2);
		if (n_classes == 2) {
			std::vector<float> y_pred_pos;
			GetCol(y_pred, y_pred_pos, 1);
			return f1_score_bin(y_pred_pos, y_true, threshold);
		} else {
			return f1_score_mul(y_pred, y_true, n_classes);
		}
	}
	float F1_score(const std::vector<int>& y_pred, const std::vector<int>& y_true, int n_classes) {
		CHECK_GE(n_classes, 2);
		if (n_classes == 2) {
			return f1_score_bin(y_pred, y_true);
		} else {
			return f1_score_mul(y_pred, y_true, n_classes);
		}
	}
	float Roc_auc_score(const std::vector<std::vector<float> >& y_pred, const std::vector<int>& y_true, int n_classes) {
		CHECK_GE(n_classes, 2);
		if (n_classes == 2) {
			std::vector<float> y_pred_pos;
			GetCol(y_pred, y_pred_pos, 1);
			return roc_auc_score(y_pred_pos, y_true);
		} else {
			return roc_auc_score_multi(y_pred, y_true, n_classes);
		}
	}
	float Pr_auc_score(const std::vector<std::vector<float> >& y_pred, const std::vector<int>& y_true, int n_classes) {
		CHECK_GE(n_classes, 2) ;
		if (n_classes == 2) {
			std::vector<float> y_pred_pos;
			GetCol(y_pred, y_pred_pos, 1);
			return pr_auc_score(y_pred_pos, y_true);
		} else {
			return pr_auc_score_multi(y_pred, y_true, n_classes);
		}
	}
	void PerformanceReport(const std::string& filename, const std::vector<std::vector<float> >& y_pred, const std::vector<int>& y_true, int n_classes, float threshold) {
		std::ofstream out;
		out.open(filename.c_str(), std::ios::out);
		CHECK(out != nullptr) << "Cannot open report file.";

		CHECK_EQ(y_pred.size(), y_true.size()) << "`y_pred` size do not match with `y_true`";

		// set format
		out << std::fixed << std::setprecision(3);

		out << "Test Set Size\t" << y_pred.size() << std::endl;
		if (n_classes == 2) 
			out << "Threshold\t" << threshold << std::endl;
		out << "Precision\t" << Precision(y_pred, y_true, n_classes, threshold) << std::endl;
		out << "Recall\t" << Recall(y_pred, y_true, n_classes, threshold) << std::endl;
		out << "F1-score\t" << F1_score(y_pred, y_true, n_classes, threshold) << std::endl;
		out << "ROC-AUC score\t" << Roc_auc_score(y_pred, y_true, n_classes) << std::endl;
		out << "Precision-Recall AUC score\t" << Pr_auc_score(y_pred, y_true, n_classes) << std::endl;
	
		out.close();
	}
};
