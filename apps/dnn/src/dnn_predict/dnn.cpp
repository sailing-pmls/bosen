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


#include "dnn.h"
#include "util.h"
#include "dnn_utils.h"
#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <cmath>

dnn::dnn(dnn_paras para,int client_id,  int num_test_data){
  num_layers=para.num_layers;
  num_units_ineach_layer=new int[num_layers];
  for(int i=0;i<num_layers;i++){
    num_units_ineach_layer[i]=para.num_units_ineach_layer[i];
  }
  this->client_id=client_id;
  this->num_test_data=num_test_data;
}







void dnn::predict(const char * model_weight_file, const char * model_bias_file, char * outputfile)
{

  float *** local_weights=new float **[num_layers-1];
  for(int l=0;l<num_layers-1;l++){
    int dim1=num_units_ineach_layer[l+1], dim2=num_units_ineach_layer[l];
    local_weights[l]=new float*[dim1];
    for(int i=0;i<dim1;i++){
      local_weights[l][i]=new float[dim2];
      memset(local_weights[l][i],0,sizeof(float)*dim2);
    }
  }
  float ** local_biases=new float*[num_layers-1];
  for(int l=0;l<num_layers-1;l++){
    local_biases[l]=new float[num_units_ineach_layer[l+1]];
    memset(local_biases[l],0,sizeof(float)*num_units_ineach_layer[l+1]);
  }

  std::ifstream infile;
  infile.open(model_weight_file);

  for(int l=0;l<num_layers-1;l++){
    int dim1=num_units_ineach_layer[l+1], dim2=num_units_ineach_layer[l];
    for(int j=0;j<dim1;j++){
      for(int i=0;i<dim2;i++){
        infile>>local_weights[l][j][i];
      }
    }
  }
  infile.close();

  infile.open(model_bias_file);
  for(int l=0;l<num_layers-1;l++){
    int dim=num_units_ineach_layer[l+1];
    for(int j=0;j<dim;j++){
      infile>>local_biases[l][j];
    }
  }
  infile.close();



  float ** z=new float*[num_layers];
  for(int i=0;i<num_layers;i++)
    z[i]=new float[num_units_ineach_layer[i]];

  std::ofstream outfile;
  outfile.open(outputfile);
  for(int i=0;i<num_test_data;i++){
    int pred=predict_single_data(i, local_weights, local_biases, z);
    outfile<<pred<<std::endl;
  }
  outfile.close();  

  for(int i=0;i<num_layers;i++)
    delete[]z[i];
  delete []z;


  //release parameter buffer
  for(int l=0;l<num_layers-1;l++){
    int dim1=num_units_ineach_layer[l+1];
    for(int i=0;i<dim1;i++)
      delete []local_weights[l][i];
    delete[]local_weights[l];
  }
  delete[]local_weights;


  for(int l=0;l<num_layers-1;l++)
    delete []local_biases[l];
  delete []local_biases;


}

int dnn::predict_single_data(int idx_data,float *** local_weights, float ** local_biases,  float ** z)
{
  copy_vec(z[0], input_features[idx_data], num_units_ineach_layer[0]);

  //forward propagation
  for(int i=1;i<num_layers;i++)
    forward_activation(i-1, local_weights[i-1], local_biases[i-1], z[i-1], z[i]);
	
  int maxid=-1;
  float maxv=-1;
  for(int i=0;i<num_units_ineach_layer[num_layers-1];i++){
    //std::cout<<z[num_layers-1][i]<<" ";
    if(z[num_layers-1][i]>maxv){
      maxv=z[num_layers-1][i];
      maxid=i;
    }
  }
  //std::cout<<std::endl;
  //std::cout<<maxid<<" "<<maxv<<std::endl;
  return maxid;
}






  

void dnn::forward_activation(int index_lower_layer, float ** local_weights, float * local_bias, float * visible, float * hidden)
{
  int num_units_hidden=num_units_ineach_layer[index_lower_layer+1];
  int num_units_visible=num_units_ineach_layer[index_lower_layer];
  matrix_vector_multiply(local_weights, visible, hidden, num_units_hidden, num_units_visible);
  add_vector(hidden, local_bias, num_units_hidden);
  if(index_lower_layer<num_layers-2)	
    activate_logistic(hidden, num_units_hidden);
  else if(index_lower_layer==num_layers-2)
    log2ori(hidden,num_units_hidden );

}








void dnn::load_data(char * data_file){
  int feadim=num_units_ineach_layer[0];

  input_features=new float*[num_test_data];
  for(int i=0;i<num_test_data;i++){
    input_features[i]=new float[feadim];
  }

  std::ifstream infile;
  infile.open(data_file);
  for(int i=0;i<num_test_data;i++){
    int label;
    infile>>label;
    for(int j=0;j<feadim;j++)
      infile>>input_features[i][j];
  }
  infile.close();  
}














