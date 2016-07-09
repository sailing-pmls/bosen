// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.10.04

#pragma once

#include "mlr_sgd_solver.hpp"
#include <boost/thread.hpp>
#include <ml/include/ml.hpp>
#include <vector>
#include <cstdint>
#include <atomic>
#include <utility>

namespace mlr {

class MLREngine {
public:
  MLREngine();
  ~MLREngine();

  void ReadData();

  int32_t GetFeatureDim() const {
    return feature_dim_;
  }

  int32_t GetNumLabels() const {
    return num_labels_;
  }

  // Can be called concurrently.
  void Start();

private:  // functions
  // Only the global head thread should call and initialize weight from
  // weight_file flag.
  void InitWeights(const std::string& weight_file);

  // Save loss up to ith evaluation (inclusive).
  void SaveLoss(int32_t up_to_ith_eval);

  // Print relevant experiment parameters.
  std::string GetExperimentInfo() const;

  // Print the ith-eval.
  std::string PrintOneEval(int32_t ith_eval);

  // Print from 0 to up_to_ith_eval (inclusive).
  std::string PrintAllEval(int32_t up_to_ith_eval);

  // Compute the classification error on the test set. This
  // is distributed. This will reset test_workload_mgr.
  void ComputeTestError(AbstractMLRSGDSolver* mlr_solver,
    petuum::ml::WorkloadManager* test_workload_mgr,
    int32_t num_data_to_use, int32_t ith_eval);

  // Compute online training error based on the first num_data_to_use. This
  // will reset workload_mgr.
  void ComputeTrainError(AbstractMLRSGDSolver* solver,
      petuum::ml::WorkloadManager* workload_mgr, int32_t num_data_to_use,
      int32_t ith_eval);

private:
  // ============== Data Variables ==================
  int32_t num_train_data_;
  int32_t num_test_data_;
  int32_t feature_dim_;
  int32_t num_labels_;  // # of classes
  int32_t num_train_eval_;  // # of train data in each eval
  bool perform_test_;
  std::string read_format_; // for both train and test.
  bool feature_one_based_;  // feature starts from 1 (train and test).
  bool label_one_based_;    // label starts from 1 (train and test).
  bool snappy_compressed_;  // file is compressed with snappy.

  // DNNEngine takes ownership of these pointers.
  std::vector<petuum::ml::AbstractFeature<float>*> train_features_;

  // train_labels_.size() == train_features_.size()
  std::vector<int32_t> train_labels_;

  std::vector<petuum::ml::AbstractFeature<float>*> test_features_;
  std::vector<int32_t> test_labels_;

  // ============= Concurrency Management ==============
  std::atomic<int32_t> thread_counter_;

  std::unique_ptr<boost::barrier> process_barrier_;

  // ============ PS Tables ============
  petuum::Table<float> loss_table_;
  petuum::Table<float> w_table_;
};

}  // namespace mlr
