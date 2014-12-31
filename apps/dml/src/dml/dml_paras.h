#ifndef DML_PARAS_H_
#define DML_PARAS_H_

struct dml_paras
{
  int src_feat_dim;//original feature dimension
  int dst_feat_dim;//target feature dimension
  float lambda;//tradeoff parameter
  float thre;//distance threshold
  float learn_rate;//learning rate
  int epoch;
  int num_total_pts;//number of total pts
  int num_simi_pairs;//number of similar pairs
  int num_diff_pairs;//num of dissimilar pairs
  
  int num_total_eval_pts;//number of total evaluation pts
  int num_eval_simi_pairs;//number of evaluation similar pairs
  int num_eval_diff_pairs;//num of evaluation dissimilar pairs

  char data_file[512];
  char simi_pair_file[512];
  char diff_pair_file[512];
  char eval_data_file[512];
  char eval_simi_pair_file[512];
  char eval_diff_pair_file[512];
   
};

void load_dml_paras(dml_paras & para, const char * file_dml_para);

void print_dml_paras(dml_paras para);

#endif