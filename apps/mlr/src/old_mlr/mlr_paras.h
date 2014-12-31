#ifndef MLR_PARAS_H_
#define MLR_PARAS_H_

struct mlr_paras
{
  int dim_feat;//feature dimension
  int num_classes;//number of classes
  int epochs;//number of epochs of stochastic gradient descent (SGD)
  float init_lr;//init learning rate
  float lr_red;//learning rate reduction ratio
  int lr_red_freq;//learning rate reduction frequency
  float pct_smps_evaluate;//percentage of samples to evaluate
  int num_iters_evaluate;//every <num_iters_evaluate> iterations, evaluate the objective function
  char data_file_list[512];
  char eval_data_meta[512];
   
};

void load_mlr_paras(mlr_paras & para, const char * file_mlr_para);

void print_mlr_paras(mlr_paras para);

#endif