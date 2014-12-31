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
#include "dnn_utils.h"


//multiplication W * a, size of W dim1 * dim2, assume a and b have been allocated
void matrix_vector_multiply(mat W, float * a, float * b, int dim1, int dim2){

  petuum::RowAccessor row_acc;

  for(int i=0;i<dim1;i++){
    W.Get(i, &row_acc);
    const petuum::DenseRow<float>& r = row_acc.Get<petuum::DenseRow<float> >();
    float sum=0;
    for(int j=0;j<dim2;j++)
      sum+=r[j]*a[j];
    b[i]=sum;
  }	
}

void add_vector(float * a, mat b, int dim){
  petuum::RowAccessor row_acc;
  b.Get(0, &row_acc);
  const petuum::DenseRow<float>& r = row_acc.Get<petuum::DenseRow<float> >();
  for(int i=0;i<dim;i++)
    a[i]+=r[i];
}


void load_mat_from_txt(float ** mat, int dim1, int dim2, char *file){
  std::ifstream infile;
  infile.open(file);
  for(int i=0;i<dim1;i++){
    for(int j=0;j<dim2;j++){
      infile>>mat[i][j];
    }
  }
  infile.close();
}

void load_vec_from_txt(int * vec, int dim, char* file){
  std::ifstream infile;
  infile.open(file);
  for(int i=0;i<dim;i++)
    infile>>vec[i];
  infile.close();
}

