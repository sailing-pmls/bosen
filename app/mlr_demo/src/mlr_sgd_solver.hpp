// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.10.04

#pragma once

#include <petuum_ps_common/include/petuum_ps.hpp>
#include <ml/include/ml.hpp>
#include <cstdint>
#include <vector>
#include <functional>
#include "abstract_mlr_sgd_solver.hpp"

namespace mlr {

struct MLRSGDSolverConfig {
  int32_t feature_dim;
  int32_t num_labels;
  int32_t w_table_num_cols;
  bool sparse_data = true;
  petuum::Table<float> w_table;
};

// The caller thread must be registered with PS.
class MLRSGDSolver : public AbstractMLRSGDSolver {
public:
  MLRSGDSolver(const MLRSGDSolverConfig& config);

 void MiniBatchSGD(
      const std::vector<petuum::ml::AbstractFeature<float>*>& features,
      const std::vector<int32_t>& labels,
      const std::vector<int32_t>& idx, double lr) override;

  // Predict the probability of each label.
  std::vector<float> Predict(
      const petuum::ml::AbstractFeature<float>& feature) const;

  // Return 0 if a prediction (of length num_labels_) correctly gives the
  // ground truth label 'label'; 0 otherwise.
  int32_t ZeroOneLoss(const std::vector<float>& prediction, int32_t label)
    const;

  // Compute cross entropy loss of a prediction (of length num_labels_) and the
  // ground truth label 'label'.
  float CrossEntropyLoss(const std::vector<float>& prediction, int32_t label)
    const;

  // Write pending updates to PS and read new w_cache_. It will use either
  // RefreshParamDense() or RefreshParamSparse().
  void RefreshParams();

  // Save the current weight in cache in libsvm format.
  void SaveWeights(const std::string& filename) const;

  float EvaluateL2RegLoss() const;

private:
  // ======== PS Tables ==========
  // The weight of each class (stored as single feature-major row).
  petuum::Table<float> w_table_;

  // Thread-cache.
  std::vector<petuum::ml::DenseFeature<float>> w_cache_;
  std::vector<std::vector<float>> w_cache_old_;

  int32_t feature_dim_; // feature dimension
  int32_t w_table_num_cols_;  // # of cols in w_table.
  int32_t num_labels_; // number of classes/labels
  int32_t w_dim_;       // dimension of w_table_ = feature_dim_ * num_labels_.

  // Specialization Functions
  std::function<float(const petuum::ml::AbstractFeature<float>&,
      const petuum::ml::AbstractFeature<float>&)> FeatureDotProductFun_;
};

}  // namespace mlr
