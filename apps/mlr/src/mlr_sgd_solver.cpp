// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.10.04

#include "mlr_sgd_solver.hpp"
#include "common.hpp"
#include <vector>
#include <cstdint>
#include <algorithm>
#include <fstream>
#include <petuum_ps_common/include/petuum_ps.hpp>
#include <ml/include/ml.hpp>

// DW: remove
#include <sstream>


namespace mlr {

MLRSGDSolver::MLRSGDSolver(const MLRSGDSolverConfig& config) :
  w_table_(config.w_table), feature_dim_(config.feature_dim),
  num_labels_(config.num_labels), w_dim_(feature_dim_ * num_labels_) {
    w_cache_.resize(num_labels_);
    w_delta_.resize(num_labels_);
    for (int i = 0; i < num_labels_; ++i) {
      if (config.sparse_weight) {
        w_cache_[i] = new petuum::ml::SparseFeature<float>(feature_dim_);
        w_delta_[i] = new petuum::ml::SparseFeature<float>(feature_dim_);
      } else {
        w_cache_[i] = new petuum::ml::DenseFeature<float>(feature_dim_);
        w_delta_[i] = new petuum::ml::DenseFeature<float>(feature_dim_);
      }
    }

    if (config.sparse_data) {
      FeatureDotProductFun_ = petuum::ml::SparseAnyFeatureDotProduct;
    } else {
      CHECK(!config.sparse_weight)
        << "Cannot use sparse weight when data is dense";
      FeatureDotProductFun_ = petuum::ml::DenseDenseFeatureDotProduct;
    }
    if (config.sparse_weight) {
      RefreshParamFun_ = &MLRSGDSolver::RefreshParamsSparse;
    } else {
      RefreshParamFun_ = &MLRSGDSolver::RefreshParamsDense;
    }
  }

MLRSGDSolver::~MLRSGDSolver() {
  for (auto p : w_cache_) {
    delete p;
  }
  for (auto p : w_delta_) {
    delete p;
  }
}

void MLRSGDSolver::RefreshParams() {
  RefreshParamFun_(*this);
}

void MLRSGDSolver::RefreshParamsDense() {
  // Write delta's to PS table.
  for (int i = 0; i < num_labels_; ++i) {
    petuum::UpdateBatch<float> w_update_batch(feature_dim_);
    auto dense_ptr = static_cast<petuum::ml::DenseFeature<float>*>(w_delta_[i]);
    const std::vector<float>& w_delta_i = dense_ptr->GetVector();
    for (int j = 0; j < feature_dim_; ++j) {
      CHECK_EQ(w_delta_i[j], w_delta_i[j]) << "nan detected.";
      w_update_batch.UpdateSet(j, j, w_delta_i[j]);
    }
    w_table_.BatchInc(i, w_update_batch);
  }

  // Zero delta.
  for (int i = 0; i < num_labels_; ++i) {
    auto dense_ptr = static_cast<petuum::ml::DenseFeature<float>*>(w_delta_[i]);
    std::vector<float>& w_delta_i = dense_ptr->GetVector();
    std::fill(w_delta_i.begin(), w_delta_i.end(), 0);
  }

  // Read w from the PS.
  for (int i = 0; i < num_labels_; ++i) {
    petuum::RowAccessor row_acc;
    w_table_.Get(i, &row_acc);
    const auto& r = row_acc.Get<petuum::DenseRow<float> >();
    r.CopyToDenseFeature(
        static_cast<petuum::ml::DenseFeature<float>*>(w_cache_[i]));
  }
}

void MLRSGDSolver::RefreshParamsSparse() {
  // Write delta's to PS table.
  for (int i = 0; i < num_labels_; ++i) {
    int32_t num_entries = w_delta_[i]->GetNumEntries();
    petuum::UpdateBatch<float> w_update_batch(num_entries);
    for (int j = 0; j < num_entries; ++j) {
      CHECK_EQ(w_delta_[i]->GetFeatureVal(j), w_delta_[i]->GetFeatureVal(j))
        << "nan detected.";
      w_update_batch.UpdateSet(j, w_delta_[i]->GetFeatureId(j),
          w_delta_[i]->GetFeatureVal(j));
    }
    w_table_.BatchInc(i, w_update_batch);
  }

  // Zero delta.
  for (int i = 0; i < num_labels_; ++i) {
    *(w_delta_[i]) = petuum::ml::SparseFeature<float>(feature_dim_);
  }

  // Read w from the PS.
  for (int i = 0; i < num_labels_; ++i) {
    petuum::RowAccessor row_acc;
    w_table_.Get(i, &row_acc);
    const auto& r = row_acc.Get<petuum::SparseFeatureRow<float> >();
    r.CopyToSparseFeature(
        static_cast<petuum::ml::SparseFeature<float>*>(w_cache_[i]));
  }
}

int32_t MLRSGDSolver::ZeroOneLoss(const std::vector<float>& prediction,
    int32_t label) const {
  int max_idx = 0;
  float max_val = prediction[0];
  for (int i = 1; i < num_labels_; ++i) {
    if (prediction[i] > max_val) {
      max_val = prediction[i];
      max_idx = i;
    }
  }
  return (max_idx == label) ? 0 : 1;
}

float MLRSGDSolver::CrossEntropyLoss(const std::vector<float>& prediction,
    int32_t label) const {
  CHECK_LE(prediction[label], 1);
  return -petuum::ml::SafeLog(prediction[label]);
}


std::vector<float> MLRSGDSolver::Predict(
    const petuum::ml::AbstractFeature<float>& feature) const {
    std::vector<float> y_vec(num_labels_);
    for (int i = 0; i < num_labels_; ++i) {
      y_vec[i] = FeatureDotProductFun_(feature, *w_cache_[i]);
    }
    petuum::ml::Softmax(&y_vec);
    return y_vec;
  }

void MLRSGDSolver::SingleDataSGD(
    const petuum::ml::AbstractFeature<float>& feature,
    int32_t label, float learning_rate) {
  std::vector<float> y_vec = Predict(feature);
  y_vec[label] -= 1.; // See Bishop PRML (2006) Eq. (4.109)

  // outer product
  for (int i = 0; i < num_labels_; ++i) {
    // w_cache_[i] += -\eta * y_vec[i] * feature
    petuum::ml::FeatureScaleAndAdd(-learning_rate * y_vec[i], feature,
        w_cache_[i]);
    petuum::ml::FeatureScaleAndAdd(-learning_rate * y_vec[i], feature,
        w_delta_[i]);
  }
}

void MLRSGDSolver::SaveWeights(const std::string& filename) const {
  std::ofstream w_stream(filename, std::ofstream::out | std::ofstream::trunc);
  CHECK(w_stream);
  // Print meta data
  w_stream << "num_labels: " << num_labels_ << std::endl;
  w_stream << "feature_dim: " << feature_dim_ << std::endl;

  for (int i = 0; i < num_labels_; ++i) {
    int num_entries = w_cache_[i]->GetNumEntries();
    for (int j = 0; j < num_entries; ++j) {
      int feature_id = w_cache_[i]->GetFeatureId(j);
      float feature_val = w_cache_[i]->GetFeatureVal(j);
      w_stream << feature_id << ":" << feature_val << " ";
    }
    w_stream << std::endl;
  }
  w_stream.close();
  LOG(INFO) << "Saved weight to " << filename;
}

}  // namespace mlr
