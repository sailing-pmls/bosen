#ifndef APPS_DML_SRC_DML_DML_PARAS_H_
#define APPS_DML_SRC_DML_DML_PARAS_H_

struct DmlParas{
  // original feature dimension
  int src_feat_dim;
  // target feature dimension
  int dst_feat_dim;
  // tradeoff parameter
  float lambda;
  // distance threshold
  float thre;
  // learning rate
  float learn_rate;
  int epoch;
  // number of total pts
  int num_total_pts;
  // number of similar pairs
  int num_simi_pairs;
  // num of dissimilar pairs
  int num_diff_pairs;
  // number of total evaluation pts
  int num_total_eval_pts;
  // size of mini batch
  int size_mb;
  // num iters to do evaluation
  int num_iters_evaluate;
  // num smps to evaluate
  int num_smps_evaluate;
  //  uniform initialization range
  float unif_x;
  float unif_y;

  char data_file[512];
  char simi_pair_file[512];
  char diff_pair_file[512];
};

void LoadDmlParas(DmlParas * para, const char * file_dml_para);

void PrintDmlParas(const DmlParas para);

#endif  // APPS_DML_SRC_DML_DML_PARAS_H_
