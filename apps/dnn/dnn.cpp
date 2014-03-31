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
#include "dnn_utils.h"
#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <stdio.h>

dnn::dnn(dnn_paras para,int client_id, int num_worker_threads, int staleness,std::string modelfile){
  num_layers=para.num_layers;
  num_units_ineach_layer=new int[num_layers];
  for(int i=0;i<num_layers;i++){
    num_units_ineach_layer[i]=para.num_units_ineach_layer[i];
  }
  num_epochs=para.epochs;
  stepsize=para.stepsize;
  size_minibatch=para.size_minibatch;
  this->num_worker_threads=num_worker_threads;
  process_barrier.reset(new boost::barrier(num_worker_threads));
  thread_counter=0;
  sprintf(model_file,"%s",modelfile.c_str());
  
  this->client_id=client_id;
  this->staleness=staleness;

  
}
void dnn::init_paras(mat *weights,mat *biases ){
  //init weights and biases randomly
  for(int i=0;i<num_layers-1;i++){
    int num_rows=num_units_ineach_layer[i+1];
    int num_cols=num_units_ineach_layer[i];
    for(int j=0;j<num_rows;j++){
       for(int k=0;k<num_cols;k++){
	   weights[i].Inc(j,k,randfloat());
	}
       biases[i].Inc(0,j,randfloat());
    }       
  }
}




void dnn::compute_error_output_layer(float * error_output_layer, float * activation_output_layer,int idx_data)
{
  int num_units_output_layer=num_units_ineach_layer[num_layers-1];
  int label=output_labels[idx_data];
  for(int k=0;k<num_units_output_layer;k++){
    if(label==k)
      error_output_layer[k]=activation_output_layer[k]-1;
    else
      error_output_layer[k]=activation_output_layer[k];	
  }
}


void dnn::sgd_mini_batch(int * idxes_batch, mat* weights, mat* biases, float *** local_weights, float ** local_biases,\	
		float *** delta_weights, float ** delta_biases, float ** z, float ** delta)
{
  //set delta_weights and delta_biases buffer to zero
  for(int l=0;l<num_layers-1;l++){
    int dim1=num_units_ineach_layer[l+1], dim2=num_units_ineach_layer[l];
    for(int i=0;i<dim1;i++)
      memset(delta_weights[l][i],0,sizeof(float)*dim2);
  }
  for(int l=0;l<num_layers-1;l++)
    memset(delta_biases[l],0,sizeof(float)*num_units_ineach_layer[l+1]);

  
  petuum::RowAccessor row_acc;
  //fetch parameters
  for(int l=0;l<num_layers-1;l++){
    int dim1=num_units_ineach_layer[l+1], dim2=num_units_ineach_layer[l];
    for(int j=0;j<dim1;j++){
      weights[l].Get(j, &row_acc);
      const petuum::DenseRow<float>& r = row_acc.Get<petuum::DenseRow<float> >();
      for(int i=0;i<dim2;i++)
        local_weights[l][j][i]=r[i];
	
    }
  }
  for(int l=0;l<num_layers-1;l++){
    int dim=num_units_ineach_layer[l+1];
    biases[l].Get(0, &row_acc);
    const petuum::DenseRow<float>& r = row_acc.Get<petuum::DenseRow<float> >();
    for(int j=0;j<dim;j++)
      local_biases[l][j]=r[j];
  }

  for(int i=0;i<size_minibatch;i++)
    compute_gradient_single_data(idxes_batch[i], local_weights,  local_biases, delta_weights, delta_biases, z,  delta );
	
  //update parameters
  float coeff_update=-stepsize/size_minibatch;
  for(int l=0;l<num_layers-1;l++){
    int dim1=num_units_ineach_layer[l+1], dim2=num_units_ineach_layer[l];
    for(int j=0;j<dim1;j++){
      for(int i=0;i<dim2;i++)
        weights[l].Inc(j,i,coeff_update*delta_weights[l][j][i]);
    }
  }
  for(int l=0;l<num_layers-1;l++){
    int dim=num_units_ineach_layer[l+1];
    for(int j=0;j<dim;j++)
      biases[l].Inc(0,j,coeff_update*delta_biases[l][j]);
  }
}


void dnn::compute_gradient_single_data(int idx_data,float *** local_weights, float ** local_biases, float *** delta_weights, \
	float ** delta_biases, float ** z, float ** delta)
{
  copy_vec(z[0], input_features[idx_data], num_units_ineach_layer[0]);

  //forward propagation
  for(int i=1;i<num_layers;i++)
    forward_activation_local(i-1, local_weights[i-1], local_biases[i-1], z[i-1], z[i]);
	
  //backward propagation
  compute_error_output_layer(delta[num_layers-2], z[num_layers-1],idx_data);
	
  for(int l=num_layers-3;l>=0;l--)
    backward_propagation_local(l, local_weights[l+1], z[l+1], delta[l], delta[l+1]);

  //accumulate delta_w
  for(int l=0;l<num_layers-1;l++){
    int dim1=num_units_ineach_layer[l+1], dim2=num_units_ineach_layer[l];
    for(int j=0;j<dim1;j++){
      for(int i=0;i<dim2;i++)
        delta_weights[l][j][i]+=delta[l][j]*z[l][i];
    }
  }
  for(int l=1;l<num_layers;l++){
    int dim=num_units_ineach_layer[l];
    for(int j=0;j<dim;j++)
      delta_biases[l-1][j]+=delta[l-1][j];
  }

}

void dnn::forward_activation_local(int index_lower_layer, float ** local_weights, float * local_bias, float * visible, float * hidden)
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

void dnn::backward_propagation_local(int index_lower_index, float ** local_weights, float * activation, \
float * error_lower_layer, float * error_higher_layer)
{
  int num_j=num_units_ineach_layer[index_lower_index+1];
  int num_k=num_units_ineach_layer[index_lower_index+2];
  memset(error_lower_layer, 0, sizeof(float)* num_j);
  for(int k=0;k<num_k;k++){
    for(int j=0;j<num_j;j++)
      error_lower_layer[j]+=local_weights[k][j]*error_higher_layer[k];
  }
  for(int j=0;j<num_j;j++){
    error_lower_layer[j]*=activation[j]*(1-activation[j]);
  }
}





void dnn::train(mat * weights, mat * biases)
{
  //allocate z and delta buffers
  float ** z=new float*[num_layers];
  for(int i=0;i<num_layers;i++)
    z[i]=new float[num_units_ineach_layer[i]];

  float ** delta=new float*[num_layers-1]; 
  for(int i=0;i<num_layers-1;i++)
    delta[i]=new float[num_units_ineach_layer[i+1]];

  //create parameter buffer
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

  float *** delta_weights=new float **[num_layers-1];
  for(int l=0;l<num_layers-1;l++){
    int dim1=num_units_ineach_layer[l+1], dim2=num_units_ineach_layer[l];
    delta_weights[l]=new float*[dim1];
    for(int i=0;i<dim1;i++){
      delta_weights[l][i]=new float[dim2];
      memset(delta_weights[l][i],0,sizeof(float)*dim2);
    }
  }
  float ** delta_biases=new float*[num_layers-1];
  for(int l=0;l<num_layers-1;l++){
    delta_biases[l]=new float[num_units_ineach_layer[l+1]];
    memset(delta_biases[l],0,sizeof(float)*num_units_ineach_layer[l+1]);
  }

  int * idxes_batch=new int[size_minibatch];

  int inner_iter=num_train_data/num_worker_threads/size_minibatch;
  srand (time(NULL));
  int it=0;
  for(int iter=0;iter<num_epochs;iter++){
    for(int i=0;i<inner_iter;i++){
      //sample mini batch
      rand_init_vec_int(idxes_batch,size_minibatch, num_train_data);
      sgd_mini_batch(idxes_batch, weights, biases, local_weights,  local_biases, delta_weights,  delta_biases, z,  delta);

      // Advance Parameter Server iteration
      petuum::TableGroup::Clock();
      it++;
      if(client_id==0&&(*thread_id)==0)
        std::cout<<"Iteration "<<it<<std::endl;

    }		
  }


  //release data

  delete []idxes_batch;

  for(int i=0;i<num_layers-1;i++)
    delete[]delta[i];
  delete[]delta;
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

  for(int l=0;l<num_layers-1;l++)
  {
    int dim1=num_units_ineach_layer[l+1];
    for(int i=0;i<dim1;i++)
      delete []delta_weights[l][i];
    delete[]delta_weights[l];
  }
  delete[]delta_weights;
  for(int l=0;l<num_layers-1;l++)
    delete []delta_biases[l];
  delete []delta_biases;


}



void dnn::load_data(std::string datafile){
  std::ifstream infile;
  infile.open(datafile.c_str());
  int feadim;
  infile>>num_train_data>>feadim;

  input_features=new float*[num_train_data];
  for(int i=0;i<num_train_data;i++){
    input_features[i]=new float[feadim];
  }
  output_labels=new int[num_train_data];
  std::string tmp;
  for(int i=0;i<num_train_data;i++){
    infile>>output_labels[i];
    infile>>tmp;
    for(int j=0;j<feadim;j++)
      infile>>input_features[i][j];
  }
  infile.close();

}

void dnn::run()
{
  if (!thread_id.get()) {
    thread_id.reset(new int(thread_counter));
    thread_counter++;
  }
  petuum::TableGroup::RegisterThread();

  mat *weights= new mat[num_layers-1];
  mat *biases=new mat[num_layers-1];
  for(int i=0;i<num_layers-1;i++){
    weights[i]=petuum::TableGroup::GetTableOrDie<float>(i);
    biases[i]=petuum::TableGroup::GetTableOrDie<float>(i+num_layers-1);
  }

  // Run additional iterations to let stale values finish propagating
  for (int iter = 0; iter < staleness; ++iter) {
    petuum::TableGroup::Clock();
  }

  if (client_id==0&&(*thread_id) == 0){
    init_paras(weights, biases);
  }
  process_barrier->wait();

  train(weights, biases);
  
  // Run additional iterations to let stale values finish propagating
  for (int iter = 0; iter < staleness; ++iter) {
    petuum::TableGroup::Clock();
  }

  //save model
  if(client_id==0&&(*thread_id)==0)
    save_model(weights, biases);

  delete[]weights;
  delete[]biases;
	
  petuum::TableGroup::DeregisterThread();

}


void dnn::save_model(mat * weights, mat *biases){
  std::ofstream outfile;
  outfile.open(model_file);
  petuum::RowAccessor row_acc;
  //fetch parameters
  for(int l=0;l<num_layers-1;l++){
    int dim1=num_units_ineach_layer[l+1], dim2=num_units_ineach_layer[l];
    outfile<<dim1<<" "<<dim2<<std::endl;
    for(int j=0;j<dim1;j++){
      weights[l].Get(j, &row_acc);
      const petuum::DenseRow<float>& r = row_acc.Get<petuum::DenseRow<float> >();
      for(int i=0;i<dim2;i++)
        outfile<<r[i]<<" ";
      outfile<<std::endl;
    }
  }
  for(int l=0;l<num_layers-1;l++){
    int dim=num_units_ineach_layer[l+1];
    outfile<<dim<<std::endl;
    biases[l].Get(0, &row_acc);
    const petuum::DenseRow<float>& r = row_acc.Get<petuum::DenseRow<float> >();
    for(int j=0;j<dim;j++)
      outfile<<r[j]<<" ";
    outfile<<std::endl;
  }
  outfile.close();
}












