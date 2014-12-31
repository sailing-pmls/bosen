

#include <fstream>
#include <string>
#include "mlr_paras.h"
#include <iostream>

void load_mlr_paras(mlr_paras & para, const char * file_mlr_para)
{
  std::ifstream infile;
  infile.open(file_mlr_para);
  std::string tmp;
  infile>>tmp>>para.dim_feat;
  infile>>tmp>>para.num_classes;
  infile>>tmp>>para.epochs;
  infile>>tmp>>para.init_lr;
  infile>>tmp>>para.lr_red;
  infile>>tmp>>para.lr_red_freq;
  infile>>tmp>>para.pct_smps_evaluate;
  infile>>tmp>>para.num_iters_evaluate;
  infile>>tmp>>para.data_file_list;
  infile>>tmp>>para.eval_data_meta;
  infile.close();

  //print the parameters to let users check
  print_mlr_paras(para);
}
void print_mlr_paras(mlr_paras para)
{
  std::cout<<"the parameters of mlr are:"<<std::endl;
  std::cout<<"dim_feat: "<<para.dim_feat<<std::endl;
  std::cout<<"num_classes: "<<para.num_classes<<std::endl;
  std::cout<<"epochs: "<<para.epochs<<std::endl;
  std::cout<<"init_lr: "<<para.init_lr<<std::endl;
  std::cout<<"lr_red: "<<para.lr_red<<std::endl;
  std::cout<<"lr_red_freq: "<<para.lr_red_freq<<std::endl;
  std::cout<<"pct_smps_evaluate: "<<para.pct_smps_evaluate<<std::endl;
  std::cout<<"num_iters_evaluate: "<<para.num_iters_evaluate<<std::endl;
  std::cout<<"data_file_list: "<<para.data_file_list<<std::endl;
  std::cout<<"eval_data_meta: "<<para.eval_data_meta<<std::endl;
  //std::cout<<": "<<<<std::endl;
}














