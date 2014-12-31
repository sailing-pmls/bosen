#ifndef APPS_DML_SRC_DML_DML_H_
#define APPS_DML_SRC_DML_DML_H_

#include "utils.hpp"
#include <iostream>
#include <petuum_ps_common/include/petuum_ps.hpp>
#include "types.hpp"

class DML {
private:
  // original feature dimension
  int src_feat_dim;
  // target feature dimension
  int dst_feat_dim;
  // tradeoff parameter
  float lambda;
  // distance threshold
  float thre;
  int size_mb;
  int num_iters_evaluate;
  int num_clients;
  int num_smps_evaluate;



  // data
  // number of total pts
  int num_total_pts;
  // all the data points
  float ** data;
  // number of similar pairs
  int num_simi_pairs;
  // num of dissimilar pairs
  int num_diff_pairs;
  // pairs labeled as similar
  pair * simi_pairs;
  // pairs labeled as dissimilar pairs
  pair * diff_pairs;


  // thread control
  // process barrier to sync threads
  boost::scoped_ptr<boost::barrier> process_barrier;
  // id of thread
  boost::thread_specific_ptr<int> thread_id;
  std::atomic<int> thread_counter;
  // misc
  // staleness value
  int staleness;
  // id of the client (machine)
  int client_id;
  int num_worker_threads;


  float PairDis(float ** L, pair p, float **data_ptr, \
     float * vec_buf_1, float * vec_buf_2);


  void Update(float ** local_paras, float **grad, float *x, float * y, int simi_update, \
  float * vec_buf_1, float * vec_buf_2);
  void Evaluate(float ** local_paras, float & simi_loss, float & diff_loss, \
     float & total_loss, float * vec_buf_1, float * vec_buf_2);
  void SaveModel(mat L, const char * model_file);

public:
  DML(int src_feat_dim, int dst_feat_dim, float lambda, \
     float thre, int client_id, int num_worker_threads, \
     int num_smps_evaluate, int staleness, int num_clients, int size_mb, int num_iters_evaluate);
  void LoadData(int num_total_data, int num_simi_pairs, int num_diff_pairs, \
     const char * data_file, const char* simi_pair_file, const char* diff_pair_file);
  void Learn(float learn_rate, int epochs, const char * model_file);
  ~DML();
};

#endif  // APPS_DML_SRC_DML_DML_H_
