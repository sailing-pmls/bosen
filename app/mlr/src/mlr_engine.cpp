// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.10.04

#include "mlr_engine.hpp"
#include "mlr_sgd_solver.hpp"
#include "lr_sgd_solver.hpp"
#include "abstract_mlr_sgd_solver.hpp"
#include "common.hpp"
#include <string>
#include <cmath>
#include <vector>
#include <cstdio>
#include <glog/logging.h>
#include <petuum_ps_common/include/petuum_ps.hpp>
#include <ml/include/ml.hpp>
#include <cstdint>
#include <fstream>
#include <io/general_fstream.hpp>

namespace mlr {

namespace {

// Save MLRSGDSolver::w_cache_ to disk. Could be time consuming if w is large.
void SaveWeights(AbstractMLRSGDSolver* mlr_solver) {
  // Save weights.
  CHECK(!FLAGS_output_file_prefix.empty());
  std::string output_filename = FLAGS_output_file_prefix + ".weight";
  mlr_solver->SaveWeights(output_filename);
}

}  // anonymous namespace

MLREngine::MLREngine() : thread_counter_(0) {
  perform_test_ = FLAGS_perform_test;
  perform_test_acc_ = FLAGS_perform_test_acc;
  num_train_eval_ = FLAGS_num_train_eval;
  process_barrier_.reset(new boost::barrier(FLAGS_num_app_threads));

  // Apepnd client_id if the train_data isn't global.
  std::string meta_file = FLAGS_train_file
    + (FLAGS_global_data ? "" : "." + std::to_string(FLAGS_client_id))
    + ".meta";
  petuum::ml::MetafileReader mreader(meta_file);
  num_train_data_ = mreader.get_int32("num_train_this_partition");
  if (FLAGS_num_train_data != 0) {
    num_train_data_ = std::min(num_train_data_, FLAGS_num_train_data);
  }
  feature_dim_ = mreader.get_int32("feature_dim");
  num_labels_ = mreader.get_int32("num_labels");
  read_format_ = mreader.get_string("format");
  feature_one_based_ = mreader.get_bool("feature_one_based");
  label_one_based_ = mreader.get_bool("label_one_based");
  snappy_compressed_ = mreader.get_bool("snappy_compressed");

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


MLREngine::~MLREngine() {
  for (auto p : train_features_) {
    delete p;
  }
  for (auto p : test_features_) {
    delete p;
  }
}


void MLREngine::ReadData() {
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
        label_one_based_, snappy_compressed_);
    if (perform_test_) {
      LOG(INFO) << "Reading test file: " << FLAGS_test_file;
      petuum::ml::ReadDataLabelLibSVM(FLAGS_test_file, feature_dim_,
          num_test_data_, &test_features_, &test_labels_,
          feature_one_based_, label_one_based_, snappy_compressed_);
    }
  }
}

void MLREngine::InitWeights(const std::string& weight_file) {
  petuum::HighResolutionTimer weight_init_timer;
  petuum::io::ifstream weight_stream(weight_file);
  CHECK(weight_stream) << "Failed to open " << weight_file;
  LOG(INFO) << "Loading weights from " << weight_file;

  // Check that num_labels and feature_dim match.
  std::string field_name;
  int32_t num_labels_weightfile;
  weight_stream >> field_name >> num_labels_weightfile;
  CHECK_EQ("num_labels:", field_name);
  CHECK_EQ(num_labels_, num_labels_weightfile);

  int32_t feature_dim_weightfile;
  weight_stream >> field_name >> feature_dim_weightfile;
  CHECK_EQ("feature_dim:", field_name);
  CHECK_EQ(feature_dim_, feature_dim_weightfile);

  // Now read the weights and put them in w_table.
  for (int i = 0; i < num_labels_; ++i) {
    petuum::UpdateBatch<float> w_update_batch(feature_dim_);
    for (int d = 0; d < feature_dim_; ++d) {
      float weight_val;
      weight_stream >> weight_val;
      w_update_batch.UpdateSet(d, d, weight_val);
    }
    w_table_.BatchInc(i, w_update_batch);
  }
  weight_stream.close();
  LOG(INFO) << "Loaded and initialized weight in "
    << weight_init_timer.elapsed();
}

void MLREngine::Start() {
  petuum::PSTableGroup::RegisterThread();

  // Initialize local thread data structures.
  int thread_id = thread_counter_++;

  int client_id = FLAGS_client_id;
  int num_clients = FLAGS_num_clients;
  int num_threads = FLAGS_num_app_threads;
  int num_epochs = FLAGS_num_epochs;
  int num_batches_per_epoch = FLAGS_num_batches_per_epoch;
  int num_secs_per_checkpoint = FLAGS_num_secs_per_checkpoint;
  float init_lr = FLAGS_init_lr;
  int num_batches_per_eval = FLAGS_num_batches_per_eval;
  bool global_data = FLAGS_global_data;
  int num_test_eval = FLAGS_num_test_eval;
  if (num_test_eval == 0) {
    num_test_eval = num_test_data_;
  }

  if (thread_id == 0) {
    loss_table_ =
      petuum::PSTableGroup::GetTableOrDie<float>(FLAGS_loss_table_id);
    w_table_ =
      petuum::PSTableGroup::GetTableOrDie<float>(FLAGS_w_table_id);
  }
  // Barrier to ensure w_table_ and loss_table_ is initialized.
  process_barrier_->wait();

  if (FLAGS_use_weight_file) {
    if (client_id == 0 && thread_id == 0) {
      InitWeights(FLAGS_weight_file);
    }
    // Barrier to ensure weights are initialized from the existing weight.
    petuum::PSTableGroup::GlobalBarrier();
  }

  // Create MLR sgd solver.
  std::unique_ptr<AbstractMLRSGDSolver> mlr_solver;
  if (num_labels_ == 2) {
    // Create LR sgd solver.
    LRSGDSolverConfig solver_config;
    solver_config.feature_dim = feature_dim_;
    solver_config.sparse_data = read_format_ == "libsvm";
    solver_config.w_table = w_table_;
    solver_config.lambda = FLAGS_lambda;
    solver_config.w_table_num_cols = FLAGS_w_table_num_cols;
    mlr_solver.reset(new LRSGDSolver(solver_config));
  } else {
    MLRSGDSolverConfig solver_config;
    solver_config.feature_dim = feature_dim_;
    solver_config.num_labels = num_labels_;
    solver_config.sparse_data = (read_format_ == "libsvm");
    solver_config.w_table = w_table_;
    solver_config.w_table_num_cols = FLAGS_w_table_num_cols;
    CHECK_EQ(0, FLAGS_lambda) << "regularization isn't supported in MLR yet.";
    mlr_solver.reset(new MLRSGDSolver(solver_config));
  }
  mlr_solver->RefreshParams();

  petuum::HighResolutionTimer total_timer;
  petuum::ml::WorkloadManagerConfig workload_mgr_config;
  workload_mgr_config.thread_id = thread_id;
  workload_mgr_config.client_id = client_id;
  workload_mgr_config.num_clients = num_clients;
  workload_mgr_config.num_threads = num_threads;
  workload_mgr_config.num_batches_per_epoch = num_batches_per_epoch;
  workload_mgr_config.num_data = num_train_data_;
  workload_mgr_config.global_data = global_data;
  petuum::ml::WorkloadManager workload_mgr(workload_mgr_config);
  // For training error.
  petuum::ml::WorkloadManager workload_mgr_train_error(workload_mgr_config);

  LOG_IF(INFO, client_id == 0 && thread_id == 0)
    << "Batch size: " << workload_mgr.GetBatchSize();

  petuum::ml::WorkloadManagerConfig test_workload_mgr_config;
  test_workload_mgr_config.thread_id = thread_id;
  test_workload_mgr_config.client_id = client_id;
  test_workload_mgr_config.num_clients = num_clients;
  test_workload_mgr_config.num_threads = num_threads;
  test_workload_mgr_config.num_batches_per_epoch = 1;
  // Need to set num_data to non-zero to avoid problem in WorkloadManager.
  test_workload_mgr_config.num_data = perform_test_ ? num_test_data_ : 10000;
  // test set is always globa (duplicated on all clients).
  test_workload_mgr_config.global_data = true;
  petuum::ml::WorkloadManager test_workload_mgr(test_workload_mgr_config);

  // It's reset after every check-pointing (saving to disk).
  petuum::HighResolutionTimer checkpoint_timer;

  float lr_decay_rate = FLAGS_lr_decay_rate;
  int32_t eval_counter = 0;
  int32_t batch_counter = 0;
  for (int epoch = 0; epoch < num_epochs; ++epoch) {
    float curr_lr = init_lr * pow(lr_decay_rate, epoch);
    workload_mgr.Restart();
    while (!workload_mgr.IsEnd()) {
      std::vector<int> minibatch_idx(workload_mgr.GetBatchSize());
      for (int i = 0; i < minibatch_idx.size(); ++i) {
        minibatch_idx[i] = workload_mgr.GetDataIdxAndAdvance();
      }
      mlr_solver->MiniBatchSGD(train_features_,
          train_labels_, minibatch_idx, curr_lr);

      CHECK(workload_mgr.IsEndOfBatch());
      petuum::PSTableGroup::Clock();
      mlr_solver->RefreshParams();
      ++batch_counter;

      if (batch_counter % num_batches_per_eval == 0) {
        petuum::HighResolutionTimer eval_timer;
        petuum::PSTableGroup::GlobalBarrier();
        mlr_solver->RefreshParams();
        ComputeTrainError(mlr_solver.get(), &workload_mgr_train_error,
            num_train_eval_, eval_counter);
        if (perform_test_) {
          ComputeTestError(mlr_solver.get(), &test_workload_mgr,
              num_test_eval, eval_counter);
        }
        if (client_id == 0 && thread_id == 0) {
          loss_table_.Inc(eval_counter, kColIdxLossTableEpoch, epoch + 1);
          loss_table_.Inc(eval_counter, kColIdxLossTableBatch,
              batch_counter);
          loss_table_.Inc(eval_counter, kColIdxLossTableTime,
              total_timer.elapsed());

          if (eval_counter > 0) {
            // Print the last eval info to overcome staleness.
            LOG(INFO) << PrintOneEval(eval_counter - 1);
            if (checkpoint_timer.elapsed() > num_secs_per_checkpoint) {
              petuum::HighResolutionTimer save_disk_timer;
              LOG(INFO) << "SaveLoss now...";
              SaveLoss(eval_counter - 1);
              SaveWeights(mlr_solver.get());
              checkpoint_timer.restart();
              LOG(INFO) << "Checkpointing finished in "
                << save_disk_timer.elapsed();
            }
          }
        }
        ++eval_counter;
      }
    }
    CHECK_EQ(0, batch_counter % num_batches_per_epoch);
  }
  petuum::PSTableGroup::GlobalBarrier();
  // Use all the train data in the last training error eval.
  ComputeTrainError(mlr_solver.get(), &workload_mgr_train_error,
      num_train_data_, eval_counter);
  if (perform_test_) {
    // Use the whole test set in the end.
    ComputeTestError(mlr_solver.get(), &test_workload_mgr,
        num_test_data_, eval_counter);
  }
  petuum::PSTableGroup::GlobalBarrier();
  if (client_id == 0 && thread_id == 0) {
    loss_table_.Inc(eval_counter, kColIdxLossTableEpoch, num_epochs);
    loss_table_.Inc(eval_counter, kColIdxLossTableBatch,
        batch_counter);
    loss_table_.Inc(eval_counter, kColIdxLossTableTime,
        total_timer.elapsed());
    LOG(INFO) << std::endl << PrintAllEval(eval_counter);
    LOG(INFO) << "Final eval: " << PrintOneEval(eval_counter);
    SaveLoss(eval_counter);
    SaveWeights(mlr_solver.get());
  }
  petuum::PSTableGroup::DeregisterThread();
}

void MLREngine::ComputeTrainError(AbstractMLRSGDSolver* mlr_solver,
    petuum::ml::WorkloadManager* workload_mgr, int32_t num_data_to_use,
    int32_t ith_eval) {
  float total_zero_one_loss = 0.;
  float total_entropy_loss = 0.;
  workload_mgr->Restart();
  int num_total = 0;
  while (!workload_mgr->IsEnd() && num_total < num_data_to_use) {
    int32_t data_idx = workload_mgr->GetDataIdxAndAdvance();
    std::vector<float> pred =
      mlr_solver->Predict(*(train_features_[data_idx]));
    total_zero_one_loss += mlr_solver->ZeroOneLoss(pred, train_labels_[data_idx]);
    total_entropy_loss += mlr_solver->CrossEntropyLoss(pred,
        train_labels_[data_idx]);
    ++num_total;
  }
  loss_table_.Inc(ith_eval, kColIdxLossTableZeroOneLoss,
      total_zero_one_loss);
  loss_table_.Inc(ith_eval, kColIdxLossTableEntropyLoss,
      total_entropy_loss);
  loss_table_.Inc(ith_eval, kColIdxLossTableNumEvalTrain,
      static_cast<float>(num_total));
}


void MLREngine::ComputeTestError(AbstractMLRSGDSolver* mlr_solver,
    petuum::ml::WorkloadManager* test_workload_mgr,
    int32_t num_data_to_use, int32_t ith_eval) {
  test_workload_mgr->Restart();
  int32_t num_error = 0;
  int32_t num_accurate = 0;
  int32_t num_total = 0;
  int i = 0;
  while (!test_workload_mgr->IsEnd() && i < num_data_to_use) {
    int32_t data_idx = test_workload_mgr->GetDataIdxAndAdvance();
    std::vector<float> pred =
      mlr_solver->Predict(*test_features_[data_idx]);
    num_error += mlr_solver->ZeroOneLoss(pred, test_labels_[data_idx]);
    num_accurate += mlr_solver->TestAccuracy(pred, test_labels_[data_idx]);
    ++num_total;
    ++i;
  }
  loss_table_.Inc(ith_eval, kColIdxLossTableTestAccuracy,
      static_cast<float>(num_accurate));
  loss_table_.Inc(ith_eval, kColIdxLossTableTestZeroOneLoss,
      static_cast<float>(num_error));
  loss_table_.Inc(ith_eval, kColIdxLossTableNumEvalTest,
      static_cast<float>(num_total));
}

std::string MLREngine::PrintOneEval(int32_t ith_eval) {
  std::stringstream output;
  petuum::RowAccessor row_acc;
  loss_table_.Get(ith_eval, &row_acc);
  const auto& loss_row = row_acc.Get<petuum::DenseRow<float> >();
  std::string test_info;
  if (perform_test_) {
    CHECK_LT(0, static_cast<int>(loss_row[kColIdxLossTableNumEvalTest]));
    std::string test_zero_one_str =
      std::to_string(loss_row[kColIdxLossTableTestZeroOneLoss] /
          loss_row[kColIdxLossTableNumEvalTest]);
    std::string num_test_str =
      std::to_string(static_cast<int>(loss_row[kColIdxLossTableNumEvalTest]));
    test_info += "test-0-1: " + test_zero_one_str
      + " num-test-used: " + num_test_str;
    if (perform_test_acc_) {
      std::string test_acc_str = std::to_string(
        loss_row[kColIdxLossTableTestAccuracy]
          / loss_row[kColIdxLossTableNumEvalTest]);
      test_info += " test-accuracy " + test_acc_str;
    }
  }
  CHECK_LT(0, static_cast<int>(loss_row[kColIdxLossTableNumEvalTrain]));
  output << loss_row[kColIdxLossTableEpoch] << " "
    << loss_row[kColIdxLossTableBatch] << " "
    << "train-0-1: " << loss_row[kColIdxLossTableZeroOneLoss] /
    loss_row[kColIdxLossTableNumEvalTrain] << " "
    << "train-entropy: " << loss_row[kColIdxLossTableEntropyLoss] /
    loss_row[kColIdxLossTableNumEvalTrain] << " "
    << "num-train-used: " << loss_row[kColIdxLossTableNumEvalTrain] << " "
    << test_info << " "
    << "time: " << loss_row[kColIdxLossTableTime] << std::endl;
  return output.str();
}

std::string MLREngine::PrintAllEval(int32_t up_to_ith_eval) {
  std::stringstream output;
  if (perform_test_) {
    if (perform_test_acc_) {
      output << "Epoch Batch Train-0-1 Train-Entropy Num-Train-Used Test-0-1 "
        << "Test-Accuracy Num-Test-Used Time" << std::endl;
    } else {
      output << "Epoch Batch Train-0-1 Train-Entropy Num-Train-Used Test-0-1 "
        << "Num-Test-Used Time" << std::endl;
    }
  } else {
    output << "Epoch Batch Train-0-1 Train-Entropy Num-Train-Used "
      << "Time" << std::endl;
  }
  for (int i = 0; i <= up_to_ith_eval; ++i) {
    petuum::RowAccessor row_acc;
    loss_table_.Get(i, &row_acc);
    const auto& loss_row = row_acc.Get<petuum::DenseRow<float> >();
    std::string test_info;
    if (perform_test_) {
      CHECK_LT(0, static_cast<int>(loss_row[kColIdxLossTableNumEvalTest]));
      std::string test_zero_one_str =
        std::to_string(loss_row[kColIdxLossTableTestZeroOneLoss]
          / loss_row[kColIdxLossTableNumEvalTest]);
      std::string num_test_str =
        std::to_string(static_cast<int>(loss_row[kColIdxLossTableNumEvalTest]));
      test_info += test_zero_one_str + " ";
      if (perform_test_acc_) {
        std::string test_acc_str = std::to_string(
          loss_row[kColIdxLossTableTestAccuracy] / 
            loss_row[kColIdxLossTableNumEvalTest]);
        test_info += test_acc_str + " ";
      }
      test_info += num_test_str;
    }
    CHECK_LT(0, static_cast<int>(loss_row[kColIdxLossTableNumEvalTrain]));
    output << loss_row[kColIdxLossTableEpoch] << " "
      << loss_row[kColIdxLossTableBatch] << " "
      << loss_row[kColIdxLossTableZeroOneLoss] /
      loss_row[kColIdxLossTableNumEvalTrain] << " "
      << loss_row[kColIdxLossTableEntropyLoss] /
      loss_row[kColIdxLossTableNumEvalTrain] << " "
      << loss_row[kColIdxLossTableNumEvalTrain] << " "
      << test_info << " "
      << loss_row[kColIdxLossTableTime] << std::endl;
  }
  return output.str();
}

void MLREngine::SaveLoss(int32_t up_to_ith_eval) {
  CHECK(!FLAGS_output_file_prefix.empty());
  std::string output_filename = FLAGS_output_file_prefix + ".loss";
  petuum::HighResolutionTimer disk_output_timer;
  petuum::io::ofstream out_stream(output_filename);
  out_stream << GetExperimentInfo();
  out_stream << PrintAllEval(up_to_ith_eval);
  out_stream.close();
  LOG(INFO) << "Loss up to " << up_to_ith_eval << " (exclusive) is saved to "
    << output_filename << " in " << disk_output_timer.elapsed();
}

std::string MLREngine::GetExperimentInfo() const {
  std::stringstream ss;
  ss << "Train set: " << FLAGS_train_file << std::endl
    << "feature_dim: " << feature_dim_ << std::endl
    << "num_labels: " << num_labels_ << std::endl
    << "num_train_data: " << num_train_data_ << std::endl
    << "num_test_data: " << num_test_data_ << std::endl
    << "num_epochs: " << FLAGS_num_epochs << std::endl
    << "num_batches_per_epoch: " << FLAGS_num_batches_per_epoch << std::endl
    << "init_lr: " << FLAGS_init_lr << std::endl
    << "lr_decay_rate: " << FLAGS_lr_decay_rate << std::endl
    << "num_batches_per_eval: " << FLAGS_num_batches_per_eval << std::endl
    << "use_weight_file: " << FLAGS_use_weight_file << std::endl
    << (FLAGS_use_weight_file ? FLAGS_weight_file + "\n" : "")
    << "num_secs_per_checkpoint: "
    << FLAGS_num_secs_per_checkpoint << std::endl
    << "staleness: " << FLAGS_staleness << std::endl
    << "num_clients: " << FLAGS_num_clients << std::endl
    << "num_app_threads: " << FLAGS_num_app_threads << std::endl
    << "num_comm_channels_per_client: "
    << FLAGS_num_comm_channels_per_client << std::endl;
  return ss.str();
}


}  // namespace mlr
