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
#include <iostream>
#include <fstream>
#include <iterator>
#include <algorithm>

namespace tree {

RandForestEngine::RandForestEngine() : num_train_data_(0),
  num_test_data_(0), feature_dim_(0),
  num_labels_(0), num_train_eval_(0), num_test_eval_(0),
  read_format_("libsvm"), feature_one_based_(0),
  label_one_based_(0), thread_counter_(0) {

  process_barrier_.reset(new boost::barrier(FLAGS_num_app_threads));
  perform_test_ = FLAGS_perform_test;
  // Params for saving prediction on test set
  save_pred_ = FLAGS_save_pred;
  pred_file_ = FLAGS_pred_file;
  if (save_pred_) {
    CHECK(!pred_file_.empty()) << "Need to specify a prediction "
      "output file path.";
  }
  // Params for saving trained trees
  save_trees_ = FLAGS_save_trees;
  output_file_ = FLAGS_output_file + ".part" + std::to_string(FLAGS_client_id);
  if (save_trees_) {
    CHECK(!output_file_.empty()) << "Need to specify an output "
      "file path.";
  }
  // Params for loading trees
  load_trees_ = FLAGS_load_trees;
  input_file_ = FLAGS_input_file;
  if (load_trees_) {
    CHECK(!input_file_.empty()) << "Need to specify an input " 
      "file path.";
  }

  if (!load_trees_) {
    SetReader();
  } else {
    // Only set meta file reader for test file
    std::string test_meta_file = FLAGS_test_file + ".meta";
    petuum::ml::MetafileReader mreader_test(test_meta_file);
    num_test_data_ = mreader_test.get_int32("num_test");
    feature_dim_ = mreader_test.get_int32("feature_dim");
    num_labels_ = mreader_test.get_int32("num_labels");
    read_format_ = mreader_test.get_string("format");
    feature_one_based_ = mreader_test.get_bool("feature_one_based");
    label_one_based_ = mreader_test.get_bool("label_one_based");
  }
}

void RandForestEngine::SetReader() {

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
  // If save trees to file, check if the file exists.
  // If exists, clear the file. If not, create the file.
  if (save_trees_) {
    std::ofstream fout;
    fout.open(output_file_, std::ios::out);
    fout.close();
  }
}

void RandForestEngine::ReadData(std::string type) {
  if (type == "train") { 
    std::string train_file = FLAGS_train_file
      + (FLAGS_global_data ? "" : "." + std::to_string(FLAGS_client_id));
    LOG(INFO) << "Reading train file: " << train_file;
    if (read_format_ == "bin") {
      petuum::ml::ReadDataLabelBinary(train_file, feature_dim_, num_train_data_,
          &train_features_, &train_labels_);
    } else if (read_format_ == "libsvm") {
      petuum::ml::ReadDataLabelLibSVM(train_file, feature_dim_, num_train_data_,
          &train_features_, &train_labels_, feature_one_based_,
          label_one_based_);
    }
  }
  if (type == "test") {
    if (read_format_ == "bin") {
      LOG(INFO) << "Reading test file: " << FLAGS_test_file;
      petuum::ml::ReadDataLabelBinary(FLAGS_test_file, feature_dim_,
          num_test_data_, &test_features_, &test_labels_);
    } else if (read_format_ == "libsvm") {
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

  // Set number of trees assigned to each thread
  int num_trees_per_thread = std::floor(static_cast<float>(FLAGS_num_trees) /
      (FLAGS_num_clients * FLAGS_num_app_threads));
  int num_left_trees = FLAGS_num_trees - FLAGS_num_clients * 
    FLAGS_num_app_threads * num_trees_per_thread;
  int num_left_clients = std::floor(static_cast<float>(num_left_trees) /
      FLAGS_num_app_threads);
  num_left_trees -= num_left_clients * FLAGS_num_app_threads;

  if (FLAGS_client_id < num_left_clients) {
      num_trees_per_thread ++;
  } else if ((FLAGS_client_id == num_left_clients) &&
    (thread_id < num_left_trees)) {
      num_trees_per_thread ++;
  }

  RandForestConfig rf_config;
  rf_config.client_id = FLAGS_client_id;
  rf_config.thread_id = thread_id;
  rf_config.num_threads = FLAGS_num_app_threads;
  rf_config.num_trees = num_trees_per_thread;
  rf_config.save_trees = save_trees_;
  rf_config.tree_config = dt_config;

  if (thread_id == 0) {
    test_vote_table_ =
      petuum::PSTableGroup::GetTableOrDie<int>(FLAGS_test_vote_table_id);
  }
  // Barrier to ensure test_vote_table_ is initialized.
  process_barrier_->wait();

  // Build the trees.
  RandForest rand_forest(rf_config);

  // Load trees from file and perform test. Use only one thread.
  if (load_trees_) {
    if (FLAGS_client_id == 0 && thread_id == 0) {
      rand_forest.LoadTrees(input_file_);
      LOG(INFO) << "Trees loaded from file.";
      // Evaluating overall test error
      float test_error = VoteOnTestData(rand_forest);
      test_error = ComputeTestError();
      petuum::PSTableGroup::GlobalBarrier();
      LOG(INFO) << "Test error: " << test_error
        << " computed on " << test_features_.size() << " test instances.";
      petuum::PSTableGroup::DeregisterThread();
    }
    return;
  }

  // Train the trees
  if (FLAGS_client_id == 0 && thread_id == 0) {
    LOG(INFO) << "Each thread train about " << num_trees_per_thread << " trees.";
  }
  rand_forest.Train();

  // Save trained trees to file
  if (save_trees_) {
    rand_forest.SaveTrees(output_file_);
  }

  // Evaluating training error on one thread of each machine.
  if (thread_id == 0) {
    float train_error = EvaluateErrorLocal(rand_forest,
        train_features_, train_labels_);
    LOG(INFO) << "client " << FLAGS_client_id << " train error: "
      << train_error << " (evaluated on "
      << num_train_data_ << " training data)";
  }

  // Test error.
  if (perform_test_) {
    float test_error = VoteOnTestData(rand_forest);
    petuum::PSTableGroup::GlobalBarrier();
    // Evaluating test error on one thread of each machine.
    if (thread_id == 0) {
      LOG(INFO) << "client " << FLAGS_client_id << " test error: "
        << test_error << " (evaluated on "
        << num_test_data_ << " test data)";
    }
    // Evaluating overall test error
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

float RandForestEngine::VoteOnTestData(const RandForest& rand_forest) {
  float error = 0.;
  for (int i = 0; i < test_features_.size(); ++i) {
    std::vector<int> votes;
    const petuum::ml::AbstractFeature<float>& x = *(test_features_[i]);
    int pred_label = rand_forest.Predict(x, &votes);
    error += (test_labels_[i] == pred_label) ? 0 : 1.;
    // add votes to test_vote_table_
    petuum::UpdateBatch<int> vote_update_batch(num_labels_);
    for (int j = 0; j < num_labels_; ++j) {
      vote_update_batch.UpdateSet(j, j, votes[j]);
    }
    test_vote_table_.BatchInc(i, vote_update_batch);
  }
  return error / test_features_.size();
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

  // Save predict result to file
  std::ofstream fpred;
  if (save_pred_) {
    fpred.open(pred_file_, std::ios::out);
    CHECK(fpred != NULL) << "Cannot open prediction output file ";
  }

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
    if (save_pred_) {
      fpred << max_label << std::endl;
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
