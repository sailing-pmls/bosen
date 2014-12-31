#ifndef MLR_H_
#define MLR_H_

#include "mlr_paras.h"
#include "util.h"
#include <petuum_ps_common/include/petuum_ps.hpp>
#include "types.h"

namespace mlr {

class MulticlassLR {
private:
  int dim_feat;//feature dimension
  int num_classes;//number of classes
  int epochs;//number of epochs of stochastic gradient descent (SGD)
  float init_lr;//init learning rate
  float lr_red;//learning rate reduction ratio
  int lr_red_freq;//learning rate reduction frequency
  float lr;//learn rate
  float pct_smps_evaluate;//percentage of samples to evaluate
  int num_iters_evaluate;//every <num_iters_evaluate> iterations, evaluate the objective function

  //thread control
  boost::scoped_ptr<boost::barrier> process_barrier;//process barrier to sync threads
  boost::thread_specific_ptr<int> thread_id;//id of thread
  std::atomic<int> thread_counter;

  //misc
  int staleness;//staleness value
  int client_id;//id of the client (machine)
  int num_worker_threads;

  float compute_cross_entropy_loss(float * output, int label);
  float compute_accuracy(float * output, int label);
  void evaluation(mat weights, float **input_features, float * output_labels,int num_data);
  void local_evaluation(float ** snap_weights, float **input_features, float * output_labels,int num_data);

public:
  MulticlassLR(mlr_paras mlr_prs, int client_id, int num_worker_threads, int staleness);
  //run the whole learning process
  void compute_ss(float **input_features, float * output_labels,int num_data, float ** eval_features, float * eval_labels, int eval_num_data);
};

}  // namespace mlr

#endif
