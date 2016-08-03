#ifndef LR_APP_H_
#define LR_APP_H_

// Logistic Regression demo

// Step 0. Include the Bosen header file and the Bosen App template file
#include <petuum_ps_common/include/ps_application.hpp>
#include <petuum_ps_common/include/petuum_ps.hpp>
#include <iostream>
#include <fstream>

class LRApp : public PsApplication {
public:
  const int32_t kDenseRowFloatTypeID = 0;
  
  int32_t sample_size = 399;
  int32_t feat_dim = 30;
  int32_t num_epoch = 1000;
  int32_t eval_epoch = 100;
  float learning_rate = 1e-4;
  float lambda = 1e-1;
  int32_t batch_size = 100;
  int32_t test_size = 170;
  
  float **x;
  float *y;
  
  float **test_x;
  float *test_y;

  float *paras;
  float *grad;
  
  // Step 1. Implement the initialization function
  void Initialize(petuum::TableGroupConfig &table_group_config);
  
  // Step 2. Implement the worker thread function
  void RunWorkerThread(int thread_id);
  
  void ReadData();
  void CalGrad();
  void PrintLoss(int epoch);
  void Eval();
};

#endif // LR_APP_H_
