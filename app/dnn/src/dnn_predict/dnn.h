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

  //data
  float ** input_features;//input features
  int num_test_data;//number of testing data	

  
  //misc
  int client_id;//id of the client (machine)


  //do forward activation
  void forward_activation(int index_lower_layer, float ** local_weights, float * local_bias, float * visible, float * hidden);

public :
  //constructor
  dnn(dnn_paras para,int client_id,  int num_test_data);

  //load input data
  void load_data(char * data_file);
  //run the whole DNN learning process
  void predict(const char * model_weight_file, const char * model_bias_file, char * outputfile);
  int predict_single_data(int idx_data,float *** local_weights, float ** local_biases,  float ** z);
};

#endif

