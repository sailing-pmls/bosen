// Logistic Regression demo

#include "lr_app.hpp"
#include <glog/logging.h>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <algorithm>

LRApp::LRApp(const LRAppConfig& config) :
train_size_(config.train_size), feat_dim_(config.feat_dim),
num_epochs_(config.num_epochs), eval_epochs_(config.eval_epochs),
learning_rate_(config.learning_rate), lambda_(config.lambda),
batch_size_(config.batch_size), test_size_(config.test_size),
w_staleness_(config.w_staleness), input_dir_(config.input_dir),
x_(config.train_size), y_(config.train_size),
test_x_(config.test_size), test_y_(config.test_size),
paras_(config.feat_dim), grad_(config.feat_dim) {
  for (int i = 0; i < train_size_; ++i) {
    x_[i].resize(feat_dim_);
  }
  for (int i = 0; i < test_size_; ++i) {
    test_x_[i].resize(feat_dim_);
  }
}

void LRApp::InitApp() {
  ReadData();
}

std::vector<petuum::TableConfig> LRApp::ConfigTables() {
  std::vector<petuum::TableConfig> table_configs;
  petuum::TableConfig config;
  config.name = "w";
  config.row_type = petuum::kDenseFloatRow;
  config.num_cols = feat_dim_;
  config.num_rows = 1;
  config.staleness = w_staleness_;
  table_configs.push_back(config);
  return table_configs;
}

// Implement the worker thread function
void LRApp::WorkerThread(int client_id, int thread_id) {
  // Step 3. Gain Table Access
  petuum::Table<float> W = GetTable<float>("w");

  // Step 4. Initialize parameters
  if (thread_id == 0) {
    petuum::DenseUpdateBatch<float> update_batch(0, feat_dim_);
    for (int i = 0; i < feat_dim_; ++i) {
      update_batch[i] = (rand() % 1001 - 500) / 500.0 * 1000.0;
    }
    W.DenseBatchInc(0, update_batch);
  }
  // Step 5. Sync after initialization using process_barrier_
  // from the parent class
  process_barrier_->wait();

  if (client_id == 0
      && thread_id == 0) {
    LOG(INFO) << "training starts";
  }

  // Step 6-8. Read & Update parameters
  for (int epoch = 0; epoch < num_epochs_; ++epoch) {
    // Step 6. Get weights from Parameter Server
    petuum::RowAccessor row_acc;
    const petuum::DenseRow<float>& r = W.Get<petuum::DenseRow<float>>(
        0, &row_acc);
    for (int i = 0; i < feat_dim_; ++i) {
      paras_[i] = r[i];
    }

    // Reset gradients
    std::fill(grad_.begin(), grad_.end(), 0.0);

    // Calculate gradients
    CalGrad();

    // Step 7. Update weights
    petuum::DenseUpdateBatch<float> update_batch(0, feat_dim_);
    learning_rate_ /= 1.005;
    for (int i = 0; i < feat_dim_; ++i) {
      update_batch[i] = 0.0 - learning_rate_ *
        (grad_[i] / batch_size_ + lambda_ * paras_[i]);
    }

    W.DenseBatchInc(0, update_batch);

    // Evaluate on training set
    if (epoch % eval_epochs_ == 0
        && client_id == 0
        && thread_id == 0) {
      PrintAcc(epoch);
    }

    // Step 8. Don't forget the Clock Tick
    petuum::PSTableGroup::Clock();
  }

  // Evaluate on test set
  if (client_id == 0
      && thread_id == 0) {
    Eval();
  }
}

void LRApp::ReadData() {
  std::string train_x = input_dir_ + "br_train_x.data";
  std::string train_y = input_dir_ + "br_train_y.data";
  std::string test_x = input_dir_ + "br_test_x.data";
  std::string test_y = input_dir_ + "br_test_y.data";

  std::ifstream fin(train_x);
  for (int i = 0; i < train_size_; ++i) {
    for (int j = 0; j < feat_dim_; ++j) {
      fin >> x_[i][j];
    }
  }
  fin.close();

  fin.open(train_y);
  for (int i = 0; i < train_size_; ++i) {
    fin >> y_[i];
  }
  fin.close();

  fin.open(test_x);
  for (int i = 0; i < test_size_; ++i) {
    for (int j = 0; j < feat_dim_; ++j) {
      fin >> test_x_[i][j];
    }
  }
  fin.close();

  fin.open(test_y);
  for (int i = 0; i < test_size_; ++i) {
    fin >> test_y_[i];
  }
  fin.close();
}

void LRApp::CalGrad() {
  for (int i = 0, sample_idx=0; i < batch_size_; ++i, ++sample_idx) {
    if (sample_idx >= train_size_)
      sample_idx -= train_size_;
    double h = 0;
    for (int j = 0; j < feat_dim_; ++j) {
      h += paras_[j]*x_[sample_idx][j];
    }
    h = 1.0 / (1.0 + exp(-h));
    for (int j = 0; j < feat_dim_; ++j) {
      grad_[j] += (h - y_[sample_idx]) * x_[sample_idx][j];
    }
  }
}

void LRApp::PrintAcc(int epoch) {
  double loss = 0.0;
  double acc = 0.0;
  for (int i = 0; i < train_size_; ++i) {
    double h = 0;
    for (int j = 0; j < feat_dim_; ++j) {
      h += paras_[j]*x_[i][j];
    }
    h = 1.0 / (1.0 + exp(-h));
    if ((h <= 0.5 && y_[i] == 0) || (h > 0.5 && y_[i] == 1)) {
      acc += 1.0;
    }
    loss -= y_[i]*log(h+1e-30) + (1-y_[i])*log(1-h+1e-30);
  }
  loss = loss / train_size_;
  for (int i = 0; i < feat_dim_; ++i) {
    loss += 0.5 * lambda_ * paras_[i] * paras_[i];
  }
  acc = acc / train_size_;
  LOG(INFO) << "iter: " << epoch << " loss: " << loss
            << " accuracy on training set: " << acc;
}

void LRApp::Eval() {
  double acc = 0;
  for (int i = 0; i < test_size_; ++i) {
    double h = 0;
    for (int j = 0; j < feat_dim_; ++j) {
      h += paras_[j]*test_x_[i][j];
    }
    h = 1.0 / (1.0 + exp(-h));
    if ((h <= 0.5 && test_y_[i] == 0) || (h > 0.5 && test_y_[i] == 1)) {
      acc += 1.0;
    }
  }
  acc = acc / test_size_;
  LOG(INFO) << "Accuracy on test set: " << acc;
}
