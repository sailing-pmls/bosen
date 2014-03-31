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

#include <petuum_ps/include/petuum_ps.hpp>
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
  boost::scoped_ptr<boost::barrier> process_barrier;
  boost::thread_specific_ptr<int> thread_id;
  std::atomic<int> thread_counter;
  
  //misc
  int staleness;
  int client_id;
  char model_file[256];

public :
  //constructor
  dnn(dnn_paras para,int client_id, int num_worker_threads, int staleness, std::string modelfile);
  //init parameters
  void init_paras(mat* weights, mat* biases);
  //compute error in output layer
  void compute_error_output_layer(float * error_output_layer, float * activation_output_layer,int idx_data);
  void backward_propagation_local(int index_lower_index, float ** local_weights, float * activation, \
	float * error_lower_layer, float * error_higher_layer);
  void forward_activation_local(int index_lower_layer, float ** local_weights, float * local_bias, float * visible, float * hidden);
  void compute_gradient_single_data(int idx_data,float *** local_weights, float ** local_biases, float *** delta_weights, \
	float ** delta_biases, float ** z, float ** delta);
  void sgd_mini_batch(int * idxes_batch, mat * weights, mat* biases, float *** local_weights, float ** local_biases,\	
		float *** delta_weights, float ** delta_biases, float ** z, float ** delta);
  void train(mat * weights, mat* biases);
  void run();
  void load_data(std::string datafile);
  void save_model(mat * weights, mat* biases);
};




#endif

