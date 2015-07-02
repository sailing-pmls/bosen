

#include <fstream>
#include <string>
#include "dml_paras.hpp"
#include <iostream>
#include <io/general_fstream.hpp>

void LoadDmlParas(DmlParas * para, const char * file_dml_para) {
  //std::ifstream infile;
  //infile.open(file_dml_para);
  petuum::io::ifstream infile(file_dml_para);
  std::string tmp;
  infile >> tmp >> para->src_feat_dim;
  infile >> tmp >> para->dst_feat_dim;
  infile >> tmp >> para->lambda;
  infile >> tmp >> para->thre;
  infile >> tmp >> para->learn_rate;
  infile >> tmp >> para->epoch;
  infile >> tmp >> para->num_total_pts;
  infile >> tmp >> para->num_simi_pairs;
  infile >> tmp >> para->num_diff_pairs;
  //infile >> tmp >> para->data_file;
  //infile >> tmp >> para->simi_pair_file;
  //infile >> tmp >> para->diff_pair_file;
  infile >> tmp >> para->size_mb;
  infile >> tmp >> para->num_iters_evaluate;
  infile >> tmp >> para->num_smps_evaluate;
  //infile >> tmp >> para->unif_x >> para->unif_y;

  infile.close();

  // print the parameters to let users check
  PrintDmlParas(*para);
}
void PrintDmlParas(DmlParas para) {
  std::cout << "src feat dim: "<< para.src_feat_dim << std::endl;
  std::cout << "dst feat dim: "<< para.dst_feat_dim<< std::endl;
  std::cout << "lambda: "<< para.lambda << std::endl;
  std::cout << "thre: "<< para.thre << std::endl;
  std::cout << "learn_rate: "<< para.learn_rate << std::endl;
  std::cout << "epoch: "<< para.epoch << std::endl;
  std::cout << "num_total_pts: "<< para.num_total_pts << std::endl;
  std::cout << "num_simi_pairs: "<< para.num_simi_pairs << std::endl;
  std::cout << "num_diff_pairs: "<< para.num_diff_pairs << std::endl;
  //std::cout << "data_file: "<< para.data_file << std::endl;
  //std::cout << "simi_pair_file: "<< para.simi_pair_file << std::endl;
  //std::cout << "diff_pair_file: "<< para.diff_pair_file << std::endl;
  std::cout << "size of minibatch: "<< para.size_mb << std::endl;
  std::cout << "num iters to evaluate: "<< para.num_iters_evaluate << std::endl;
  std::cout << "num smps_evaluate: "<< para.num_smps_evaluate << std::endl;
  //std::cout << "uniform initialization range: "<< para.unif_x << para.unif_y << std::endl;
}














