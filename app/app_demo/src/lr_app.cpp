// Logistic Regression demo

#include "lr_app.hpp"
#include <glog/logging.h>
#include <iostream>
#include <fstream>
#include <cstdint>

LRApp::LRApp(const LRAppConfig& config) :
train_size_(config.train_size), feat_dim_(config.feat_dim),
num_epochs_(config.num_epochs), eval_epochs_(config.eval_epochs),
  learning_rate_(config.learning_rate),
lambda_(config.lambda), batch_size_(config.batch_size),
test_size_(config.test_size), w_staleness_(config.w_staleness) { }

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

void LRApp::InitApp() {
  ReadData();
}

// Step 2. Implement the worker thread function
void LRApp::WorkerThread(int thread_id) {
  // Step 2.0. Gain Table Access
  petuum::Table<float> W = GetTable<float>("w");

  // Step 2.1. Initialize parameters
  if (thread_id == 0) {
    petuum::DenseUpdateBatch<float> update_batch(0, feat_dim_);
    for (int i = 0; i < feat_dim_; ++i) {
      update_batch[i] = (rand() % 1001 - 500) / 500.0;
    }
    W.DenseBatchInc(0, update_batch);
  }
  // Sync after initialization using process_barrier_ from the parent class
  process_barrier_->wait();

  if (thread_id == 0) {
    LOG(INFO) << "training starts";
  }

  // Create Local parameters & gradients buffer
  paras_ = new float[feat_dim_];
  grad_ = new float[feat_dim_];

  // Step 2.2. Read & Update parameters
  for (int epoch = 0; epoch < num_epochs_; ++epoch) {
    // Get weights from Parameter Server
    petuum::RowAccessor row_acc;
    const petuum::DenseRow<float>& r = W.Get<petuum::DenseRow<float>>(
        0, &row_acc);
    for (int i = 0; i < feat_dim_; ++i) {
      paras_[i] = r[i];
    }

    // Comment(wdai): Use std::fill from #include <algorithm>. Also
    // use std::vector> for grad_
    //
    // Reset gradients
    memset(grad_, 0, feat_dim_ * sizeof(float));

    // Calculate gradients
    CalGrad();

    // Update weights
    petuum::DenseUpdateBatch<float> update_batch(0, feat_dim_);
    for (int i = 0; i < feat_dim_; ++i) {
      update_batch[i] = - learning_rate_ *
        (grad_[i] / batch_size_ + lambda_ * paras_[i]);
    }
    W.DenseBatchInc(0, update_batch);

    // Evaluate on training set
    if (epoch % eval_epochs_ == 0
        && (thread_id + epoch/eval_epochs_) % 2 == 0) {
      PrintLoss(epoch);
    }

    // Step 2.3. Don't forget the Clock Tick
    petuum::PSTableGroup::Clock();
  }

  // Evaluate on test set
  if (thread_id == 0) {
    Eval();
  }
}

void LRApp::ReadData() {
  x_ = new float*[train_size_];
  for (int i = 0; i < train_size_; ++i) x_[i] = new float[feat_dim_];
  y_ = new float[train_size_];

  std::ifstream fin("./input/br_train_x.data");
  for (int i = 0; i < train_size_; ++i)
    for (int j = 0; j < feat_dim_; ++j) fin >> x_[i][j];
  fin.close();

  fin.open("./input/br_train_y.data");
  for (int i = 0; i < train_size_; ++i) fin >> y_[i];
  fin.close();

  test_x_ = new float*[test_size_];
  for (int i = 0; i < test_size_; ++i) test_x_[i] = new float[feat_dim_];
  test_y_ = new float[test_size_];

  fin.open("./input/br_train_x.data");
  for (int i = 0; i < test_size_; ++i)
    for (int j = 0; j < feat_dim_; ++j) fin >> test_x_[i][j];
  fin.close();

  fin.open("./input/br_train_y.data");
  for (int i = 0; i < test_size_; ++i) fin >> test_y_[i];
  fin.close();
}

void LRApp::CalGrad() {
  for (int i = 0; i < batch_size_; ++i) {
    int k = rand() % train_size_;
    float h = 0;
    for (int j = 0; j < feat_dim_; ++j) h += paras_[j]*x_[k][j];
    h = 1.0 / (1.0 + exp(-h));
    for (int j = 0; j < feat_dim_; ++j) {
      grad_[j] += (h - y_[k]) * x_[k][j];
    }
  }
}

void LRApp::PrintLoss(int epoch) {
  double loss = 0;
  for (int i = 0; i < train_size_; ++i) {
    double h = 0;
    for (int j = 0; j < feat_dim_; ++j) h += paras_[j]*x_[i][j];
    h = 1.0 / (1.0 + exp(-h));
    if (y_[i] > 0.5) loss -= log(h);
    else loss -= log(1-h);
  }
  loss = loss / train_size_;
  LOG(INFO) << "iter: " << epoch << " loss: " << loss;
}

void LRApp::Eval() {
  float acc = 0;
  for (int i = 0; i < test_size_; ++i) {
    float h = 0;
    for (int j = 0; j < feat_dim_; ++j) h += paras_[j]*test_x_[i][j];
    h = 1.0 / (1.0 + exp(-h));
    if ((h < 0.5 && test_y_[i] == 0) || (h > 0.5 && test_y_[i] == 1)) {
      acc += 1.0;
    }
  }
  acc = acc / test_size_;
  LOG(INFO) << "Accuracy on test set: " << acc;
}
