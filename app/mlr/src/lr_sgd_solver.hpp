// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2015.02.04

#pragma once

#include <petuum_ps_common/include/petuum_ps.hpp>
#include <ml/include/ml.hpp>
#include <cstdint>
#include <vector>
#include "abstract_mlr_sgd_solver.hpp"

namespace mlr {

struct LRSGDSolverConfig {
  int32_t feature_dim;

  petuum::Table<float> w_table;
  int32_t w_table_num_cols;
  bool sparse_data = true;

  float lambda = 0;   // l2 regularization parameter
};

// Binary solver. Does not support sparse LR parameters. Labels y \ in {0, 1}.
// NOT {-1, 1}
class LRSGDSolver : public AbstractMLRSGDSolver {
public:
  LRSGDSolver(const LRSGDSolverConfig& config);

  // Compute gradient using on data pointed by idx and store gradient
  // internally.
  void MiniBatchSGD(
      const std::vector<petuum::ml::AbstractFeature<float>*>& features,
      const std::vector<int32_t>& label, const std::vector<int32_t>& idx,
      double lr);

  // Predict the probability of each label.
  std::vector<float> Predict(
      const petuum::ml::AbstractFeature<float>& feature) const;

  // Return 1 if a prediction (of length num_labels_) correctly gives the
  // ground truth label 'label'; 0 otherwise.
  int32_t ZeroOneLoss(const std::vector<float>& prediction, int32_t label)
    const;

  // Return 1 if a prediction (of length num_labels_) correctly gives the
  // ground truth label 'label'; 0 otherwise.
  int32_t TestAccuracy(const std::vector<float>& prediction, int32_t label)
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

  // Apply L2 decay if applicable.
  void Update(double batch_learning_rate);

private:
  // ======== PS Tables ==========
  // The weight of each class (stored as single feature-major row).
  petuum::Table<float> w_table_;

  // Thread-cache.
  petuum::ml::DenseFeature<float> w_cache_;
  std::vector<float> w_cache_old_;
  //petuum::ml::DenseDecayFeature<float> w_delta_;

  int32_t feature_dim_; // feature dimension
  // feature_dim % w_table_num_cols might not be 0
  int32_t w_table_num_cols_;  // # of cols in w_table.
  float lambda_;   // l2 regularization parameter

  // Specialization Functions
  std::function<float(const petuum::ml::AbstractFeature<float>&,
      const petuum::ml::AbstractFeature<float>&)> FeatureDotProductFun_;
};

}  // namespace mlr
