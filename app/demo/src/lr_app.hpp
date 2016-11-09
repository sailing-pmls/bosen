#pragma once

// Logistic Regression demo

// Step 0. Include the Bosen header file and the Bosen App template file
#include <petuum_ps_common/include/ps_app.hpp>
#include <petuum_ps_common/include/petuum_ps.hpp>
#include <cstdint>
#include <vector>
#include <string>

struct LRAppConfig {
  std::string input_dir;
  int64_t train_size = 399;
  int64_t feat_dim = 30;
  int num_epochs = 10000;
  int eval_epochs = 100;
  float learning_rate = 1.0;
  float lambda = 1e-1;
  int batch_size = 100;
  int64_t test_size = 170;
  int w_staleness = 0;
};

class LRApp : public petuum::PsApp {
public:
  LRApp(const LRAppConfig& config);

  // Step 1. Initialization executed by one thread (cannot access table).
  void InitApp() override;

  // Step 2. 
  std::vector<petuum::TableConfig> ConfigTables() override;

  // Step 3-8. Implement the worker thread function
  void WorkerThread(int client_id, int thread_id) override;

private:
  void ReadData();
  void CalGrad();
  void PrintAcc(int epoch);
  void Eval();

private:
  int64_t train_size_;
  int64_t feat_dim_;
  int num_epochs_;
  int eval_epochs_;
  float learning_rate_;
  float lambda_;
  int batch_size_;
  int64_t test_size_;
  int w_staleness_;
  std::string input_dir_;

  std::vector<std::vector<float> > x_;
  std::vector<float> y_;

  std::vector<std::vector<float> > test_x_;
  std::vector<float> test_y_;

  std::vector<float> paras_;
  std::vector<double> grad_;
};
