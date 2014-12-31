
#include "utils.h"
#include <iostream>
#include <petuum_ps_common/include/petuum_ps.hpp>
#include "types.h"

class dml
{
private:
  int src_feat_dim;//original feature dimension
  int dst_feat_dim;//target feature dimension
  float lambda;//tradeoff parameter
  float thre;//distance threshold

  //data
  int num_total_pts;//number of total pts
  float ** data;//all the data points
  int num_simi_pairs;//number of similar pairs
  int num_diff_pairs;//num of dissimilar pairs
  pair * simi_pairs;//pairs labeled as similar
  pair * diff_pairs;//pairs labeled as dissimilar pairs

  //evaluation data
  int num_total_eval_pts;//number of total evaluation pts
  float ** eval_data;//all the evaluation data points
  int num_eval_simi_pairs;//number of evaluation similar pairs
  int num_eval_diff_pairs;//num of evaluation dissimilar pairs
  pair * eval_simi_pairs;//pairs labeled as similar for evaluation
  pair * eval_diff_pairs;//pairs labeled as dissimilar pairs for evaluation

  //thread control
  boost::scoped_ptr<boost::barrier> process_barrier;//process barrier to sync threads
  boost::thread_specific_ptr<int> thread_id;//id of thread
  std::atomic<int> thread_counter;
  
  //misc
  int staleness;//staleness value
  int client_id;//id of the client (machine)
  int num_worker_threads;


  float pair_dis(float ** L, pair p, float **data_ptr, float * vec_buf_1, float * vec_buf_2);


  void update(mat L, float *x, float * y,int simi_update,float learn_rate, float * vec_buf_1, float * vec_buf_2);
  void evaluate(mat L, float & simi_loss, float & diff_loss, float & total_loss, float * vec_buf_1, float * vec_buf_2);
public:
  dml(int src_feat_dim, int dst_feat_dim, float lambda, float thre, int client_id, int num_worker_threads, int staleness);
  void load_data(int num_total_data, int num_simi_pairs, int num_diff_pairs, char * data_file, char* simi_pair_file, char* diff_pair_file);
  void load_eval_data(int num_total_eval_data, int num_eval_simi_pairs, int num_eval_diff_pairs, char * eval_data_file, char* eval_simi_pair_file, char* eval_diff_pair_file);
  void learn(float learn_rate, int epochs);
  ~dml();
};
