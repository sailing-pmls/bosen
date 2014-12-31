// Author: Jiesi Zhao (jiesizhao0423@gmail.com), Wei Dai (wdai@cs.cmu.edu)
// Date: 2014.11.6

#include "common.hpp"
#include "decision_tree.hpp"
#include "rand_forest.hpp"
#include "rand_forest_engine.hpp"
#include <ml/include/ml.hpp>
#include <vector>
#include <cstdint>
#include <atomic>
#include <cmath>

namespace tree {

RandForestEngine::RandForestEngine() : thread_counter_(0) {
  perform_test_ = FLAGS_perform_test;
  process_barrier_.reset(new boost::barrier(FLAGS_num_app_threads));

  // Append client_id if the train_data isn't global.
  std::string meta_file = FLAGS_train_file
    + (FLAGS_global_data ? "" : "." + std::to_string(FLAGS_client_id))
    + ".meta";
  petuum::ml::MetafileReader mreader(meta_file);
  num_train_data_ = mreader.get_int32("num_train_this_partition");
  num_train_data_ = std::max(num_train_data_, FLAGS_num_train_data);
  feature_dim_ = mreader.get_int32("feature_dim");
  num_labels_ = mreader.get_int32("num_labels");
  read_format_ = mreader.get_string("format");
  feature_one_based_ = mreader.get_bool("feature_one_based");
  label_one_based_ = mreader.get_bool("label_one_based");

  // Read test meta file.
  if (perform_test_) {
    std::string test_meta_file = FLAGS_test_file + ".meta";
    petuum::ml::MetafileReader mreader_test(test_meta_file);
    num_test_data_ = mreader_test.get_int32("num_test");
    CHECK_EQ(feature_dim_, mreader_test.get_int32("feature_dim"));
    CHECK_EQ(num_labels_, mreader_test.get_int32("num_labels"));
    CHECK_EQ(read_format_, mreader_test.get_string("format"));
    CHECK_EQ(feature_one_based_, mreader_test.get_bool("feature_one_based"));
    CHECK_EQ(label_one_based_, mreader_test.get_bool("label_one_based"));
  }
}

void RandForestEngine::ReadData() {
  std::string train_file = FLAGS_train_file
    + (FLAGS_global_data ? "" : "." + std::to_string(FLAGS_client_id));
  LOG(INFO) << "Reading train file: " << train_file;
  if (read_format_ == "bin") {
    petuum::ml::ReadDataLabelBinary(train_file, feature_dim_, num_train_data_,
        &train_features_, &train_labels_);
    if (perform_test_) {
      LOG(INFO) << "Reading test file: " << FLAGS_test_file;
      petuum::ml::ReadDataLabelBinary(FLAGS_test_file, feature_dim_,
          num_test_data_, &test_features_, &test_labels_);
    }
  } else if (read_format_ == "libsvm") {
    petuum::ml::ReadDataLabelLibSVM(train_file, feature_dim_, num_train_data_,
        &train_features_, &train_labels_, feature_one_based_,
        label_one_based_);
    if (perform_test_) {
      LOG(INFO) << "Reading test file: " << FLAGS_test_file;
      petuum::ml::ReadDataLabelLibSVM(FLAGS_test_file, feature_dim_,
          num_test_data_, &test_features_, &test_labels_,
          feature_one_based_, label_one_based_);
    }
  }
}

void RandForestEngine::Start() {
  petuum::PSTableGroup::RegisterThread();

  // Initialize local thread data structures.
  int thread_id = thread_counter_++;

  DecisionTreeConfig dt_config;
  dt_config.max_depth = FLAGS_max_depth;
  dt_config.num_data_subsample = FLAGS_num_data_subsample;
  dt_config.num_features_subsample = FLAGS_num_features_subsample;
  dt_config.num_labels = num_labels_;
  dt_config.feature_dim = feature_dim_;
  dt_config.features = &train_features_;
  dt_config.labels = &train_labels_;

  int num_trees_per_thread = std::ceil(static_cast<float>(FLAGS_num_trees) /
      (FLAGS_num_clients * FLAGS_num_app_threads));
  RandForestConfig rf_config;
  rf_config.num_trees = num_trees_per_thread;
  rf_config.tree_config = dt_config;

  if (thread_id == 0) {
    test_vote_table_ =
      petuum::PSTableGroup::GetTableOrDie<int>(FLAGS_test_vote_table_id);
  }
  // Barrier to ensure test_vote_table_ is initialized.
  process_barrier_->wait();

  // Build the trees.
  RandForest rand_forest(rf_config);

  // Evaluating training error on one thread of each machine.
  if (thread_id == 0) {
    float train_error = EvaluateErrorLocal(rand_forest,
        train_features_, train_labels_);
    LOG(INFO) << "client " << FLAGS_client_id << " train error: "
      << train_error << " (evaluated on "
      << num_train_data_ << " training data)";

    float test_error = EvaluateErrorLocal(rand_forest,
        test_features_, test_labels_);
    LOG(INFO) << "client " << FLAGS_client_id << " test error: "
      << test_error << " (evaluated on "
      << num_test_data_ << " test data)";
  }

  // Test error.
  if (FLAGS_perform_test) {
    VoteOnTestData(rand_forest);
    petuum::PSTableGroup::GlobalBarrier();
    if (FLAGS_client_id == 0 && thread_id == 0) {
      float test_error = ComputeTestError();
      LOG(INFO) << "Test error: " << test_error
        << " computed on " << test_features_.size() << " test instances.";
    }
  }

  petuum::PSTableGroup::DeregisterThread();
}

// =========== Private Functions =============

float RandForestEngine::EvaluateErrorLocal(const RandForest& rand_forest,
    const std::vector<petuum::ml::AbstractFeature<float>*>& features,
    const std::vector<int32_t>& labels) {
  float error = 0.;
  for (int i = 0; i < features.size(); ++i) {
    const petuum::ml::AbstractFeature<float>& x = *(features[i]);
    int pred_label = rand_forest.Predict(x);
    error += (labels[i] == pred_label) ? 0 : 1.;
  }
  return error / features.size();
}

void RandForestEngine::VoteOnTestData(const RandForest& rand_forest) {
  for (int i = 0; i < test_features_.size(); ++i) {
    std::vector<int> votes;
    const petuum::ml::AbstractFeature<float>& x = *(test_features_[i]);
    rand_forest.Predict(x, &votes);
    // add votes to test_vote_table_
    petuum::UpdateBatch<int> vote_update_batch(num_labels_);
    for (int j = 0; j < num_labels_; ++j) {
      vote_update_batch.UpdateSet(j, j, votes[j]);
    }
    test_vote_table_.BatchInc(i, vote_update_batch);
  }
}

namespace {

int SumVector(const std::vector<int> vec) {
  int sum = 0;
  for (const auto& elem : vec) {
    sum += elem;
  }
  return sum;
}

}  // anonymous namespace

float RandForestEngine::ComputeTestError() {
  // Head thread collects the votes.
  float error = 0.;
  int num_trees = 0;
  for (int i = 0; i < test_features_.size(); ++i) {
    petuum::RowAccessor row_acc;
    test_vote_table_.Get(i, &row_acc);
    const auto& test_vote_row = row_acc.Get<petuum::DenseRow<int> >();
    std::vector<int> test_votes;
    test_vote_row.CopyToVector(&test_votes);
    int max_label = 0;
    for (int j = 1; j < num_labels_; ++j) {
      if (test_votes[max_label] < test_votes[j]) {
        max_label = j;
      }
    }
    error += (test_labels_[i] == max_label) ? 0 : 1.;

    if (i == 0) {
      num_trees = SumVector(test_votes);
    } else {
      CHECK_EQ(num_trees, SumVector(test_votes));
    }
  }
  LOG(INFO) << "Test using " << num_trees << " trees.";
  return error / test_features_.size();
}

}  // namespace tree
