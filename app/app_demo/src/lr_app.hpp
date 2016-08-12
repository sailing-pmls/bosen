#pragma once

// Logistic Regression demo

// Step 0. Include the Bosen header file and the Bosen App template file
#include <petuum_ps_common/include/ps_application.hpp>
#include <petuum_ps_common/include/petuum_ps.hpp>
#include <cstdint>
#include <vector>

struct LRAppConfig {
  int64_t train_size = 399;
  int64_t feat_dim = 30;
  int num_epochs = 1000;
  int eval_epochs = 100;
  float learning_rate = 1e-4;
  float lambda = 1e-1;
  int batch_size = 100;
  int64_t test_size = 170;
  int w_staleness = 0;
};

class LRApp : public petuum::PsApplication {
public:
  LRApp(const LRAppConfig& config);

  std::vector<petuum::TableConfig> ConfigTables() override;

  // Initialization executed by one thread (cannot access table).
  void InitApp() override;

  // Step 2. Implement the worker thread function
  void WorkerThread(int thread_id) override;

private:
  void ReadData();
  void CalGrad();
  void PrintLoss(int epoch);
  void Eval();

private:
  // All member fields need to end with '_'
  int64_t train_size_;
  int64_t feat_dim_;
  int num_epochs_;
  int eval_epochs_;
  float learning_rate_;
  float lambda_;
  int batch_size_;
  int64_t test_size_;
  int w_staleness_;

  // TODO(wdai): Never use C array. Use C++ vector instead. Currently the
  // code has memory leak (no delete in destructor).
  float **x_;
  float *y_;

  float **test_x_;
  float *test_y_;

  float *paras_;
  float *grad_;
};
