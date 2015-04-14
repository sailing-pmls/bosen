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
#ifndef UTIL_H_
#define UTIL_H_


#include <cmath>
#include <stdlib.h>
#include <fstream>
#include <time.h>

int myrandom (int i);
//get time
float get_time();
//random int
int randint(int maxvalue);
//random float
float randfloat();
//logistic function
float logistic(float x);
//component-wise activation using logistic function
void activate_logistic(float * x, int dim);
//multiplication W * a, size of W dim1 * dim2, assume a and b have been allocated
void matrix_vector_multiply(float ** W, float * a, float * b, int dim1, int dim2);
void matrix_vector_multiply_colwise(float ** W, float * a, float * b, int dim1, int dim2);
//copy vectors
void copy_vec(float * a, float * b, int dim);
void copy_mat(float ** a, float ** b, int dim1, int dim2);
//transpose matrix
void transpose(float ** weights, float ** weights_transpose,int dim1, int dim2);
// a= a+b;
void add_vector(float * a, float * b, int dim);
//init vector randomly
void rand_init_vec(float * a, int dim);
void rand_init_vec_int(int * a, int dim, int max_int);
//init matrix randomly
void rand_init_mat(float ** a, int dim1, int dim2);

//save matrix to txt file
void save_mat_txt(char * filename, float ** mat, int rows,int cols);
//save vector to txt file
void save_vec_txt(char * filename, float *vec, int len);
//multinomial normalization in log domain
float log_sum(float log_a, float log_b);
float log_sum_vec(float * logvec,int D);
void log2ori(float * logvec, float * orivec,int D );
void log2ori(float *logvec, int D);
float dis_l2(float *a, float *b,int D);

//load matrix from binary file
template<typename T>
void load_bin_mat(T ** a, int dim1, int dim2, const char *file)
{
	FILE * fp=fopen(file,"rb");
	for(int i=0;i<dim1;i++)
	{
		fread(a[i],sizeof(T),dim2,fp); 
	}
	fclose(fp);
}
//load vector from binary file
template<typename T>
void load_bin_vec(T * a, int dim, const char *file)
{
	FILE * fp=fopen(file,"rb");
	fread(a,sizeof(T),dim,fp); 
	fclose(fp);
}

#endif
