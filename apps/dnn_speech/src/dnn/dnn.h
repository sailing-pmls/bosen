// Copyright (c) 2014, Sailing Lab
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the <ORGANIZATION> nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//author: Pengtao Xie (pengtaox@cs.cmu.edu)
#ifndef DNN_H_
#define DNN_H_

#include <petuum_ps_common/include/petuum_ps.hpp>


#include "paras.h"
#include "util.h"
#include "types.h"
#include <atomic>
#include <string>

//class of deep neural network
class dnn
{
private :
  //meta parameters
  int num_layers;//number of layers
  int * num_units_ineach_layer;//number of unis in each layer
  int num_epochs;//number of epochs in stochastic gradient descent (SGD)
  float stepsize;//stepsize of SGD
  int num_worker_threads;//number of worker threads
  int size_minibatch;//mini batch size

  //data
  float ** input_features;//input features
  int * output_labels;//labels
  int num_train_data;//number of training data	

  //thread control
  boost::scoped_ptr<boost::barrier> process_barrier;//process barrier to sync threads
  boost::thread_specific_ptr<int> thread_id;//id of thread
  std::atomic<int> thread_counter;
  
  //misc
  int staleness;//staleness value
  int client_id;//id of the client (machine)

  int num_smps_evaluate;//when evaluating objective function, randomly sample <num_smps_evaluate> points to evaluate the objective function
  int num_iters_evaluate;//every <num_iters_evaluate> iterations, evaluate the objective function

  //do forward activation
  void forward_activation(int index_lower_layer, float ** local_weights, float * local_bias, float * visible, float * hidden);
  //compute error in output layer
  void compute_error_output_layer(float * error_output_layer, float * activation_output_layer,int idx_data);
  //compute backward error
  void backward_error_computation(int index_lower_index, float ** local_weights, float * activation, float * error_lower_layer, float * error_higher_layer);
  //compute the gradient of a single data point
  void compute_gradient_single_data(int idx_data,float *** local_weights, float ** local_biases, float *** delta_weights, float ** delta_biases, float ** z, float ** delta);
  //stochastic gradient descent on a mini batch
  void sgd_mini_batch(int * idxes_batch, mat * weights, mat* biases, float *** local_weights, float ** local_biases, float *** delta_weights, float ** delta_biases, float ** z, float ** delta, int ** rand_idxes_weight, int * rand_idxes_bias);
  //compute loss over the whole batch
  float compute_loss( float*** weights, float** biases);
  //compute the cross entropy loss
  float compute_cross_entropy_loss(float * output, int idx_data);
  //train neural network
  void train(mat * weights, mat* biases);
  //save model
  //void save_model(mat * weights, mat *biases, const char * mdl_weight_file, const char * mdl_biases_file);
  void save_model_kaldiformat(mat * weights, mat *biases, const char * mdl_para_file);
public :
  //constructor
  dnn(dnn_paras para,int client_id, int num_worker_threads, int staleness, int num_train_data);
  //init parameters
  void init_paras(mat* weights, mat* biases);
  //load input data
  void load_data(char * data_file);
  //run the whole DNN learning process
  //void run(std::string model_weight_file, std::string model_bias_file);
  void run(std::string model_para_file);
};

#endif

