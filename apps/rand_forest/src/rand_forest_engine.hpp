// Author: Jiesi Zhao (jiesizhao0423@gmail.com), Wei Dai (wdai@cs.cmu.edu)
// Date: 2014.11.6

#pragma once

#include "decision_tree.hpp"
#include "rand_forest.hpp"
#include <ml/include/ml.hpp>
#include <petuum_ps_common/include/petuum_ps.hpp>
#include <boost/thread.hpp>
#include <vector>
#include <cstdint>
#include <atomic>
#include <utility>

namespace tree {

class RandForestEngine {
public:
  RandForestEngine();

  void ReadData();

  int GetNumLabels() const {
    return num_labels_;
  }

  int GetNumTestData() const {
    return num_test_data_;
  }

  // Can be called concurrently.
  void Start();

private:  // private methods
  static float EvaluateErrorLocal(const RandForest& rand_forest,
      const std::vector<petuum::ml::AbstractFeature<float>*>& features,
     const std::vector<int32_t>& labels);

  // Register label votes on test data on test_vote_table_.
  void VoteOnTestData(const RandForest& rand_forest);

  // Only head thread should call this to collect the votes and compute test
  // error.
  float ComputeTestError();

private:
  // ============== Data Variables ==================
  int32_t num_train_data_;
  int32_t num_test_data_;
  int32_t feature_dim_;
  int32_t num_labels_;  // # of classes
  int32_t num_train_eval_;  // # of train data in each eval
  int32_t num_test_eval_;  // # of test data in each eval
  bool perform_test_;
  std::string read_format_; // for both train and test.
  bool feature_one_based_;  // feature starts from 1 (train and test).
  bool label_one_based_;    // label starts from 1 (train and test).

  // Each feature is a float array of length num_units_each_layer_[0].
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
  petuum::Table<int> test_vote_table_;
};

}  // namespace tree
