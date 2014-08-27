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
#include "dnn.h"
#include "util.h"
#include <iostream>
#include <time.h>

int myrandom (int i) 
{ 
  std::srand(std::time(0));
  return std::rand()%i;
}

//get time
float get_time() {
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC, &start);
  return (start.tv_sec + start.tv_nsec/1000000000.0);
}
//random int
int randint(int maxvalue)
{
	return rand()%maxvalue;
}
//random double
float randfloat()
{
	return ((rand()%1001)*2-1000)/10000.0;
}
//logistic function
float logistic(float x)
{
	return 1/(1+exp(-x));
}
//component-wise activation using logistic function
void activate_logistic(float * x, int dim)
{
	for(int i=0;i<dim;i++)
		x[i]=logistic(x[i]);
}
//multiplication W * a, size of W dim1 * dim2, assume a and b have been allocated
void matrix_vector_multiply(float ** W, float * a, float * b, int dim1, int dim2)
{
	for(int i=0;i<dim1;i++)
	{
		float sum=0;
		for(int j=0;j<dim2;j++)
			sum+=W[i][j]*a[j];
		b[i]=sum;
	}
}
void matrix_vector_multiply_colwise(float ** W, float * a, float * b, int dim1, int dim2)
{
	for(int i=0;i<dim2;i++)
	{
		float sum=0;
		for(int j=0;j<dim1;j++)
			sum+=W[j][i]*a[j];
		b[i]=sum;
	}
}


//copy vectors
void copy_vec(float * a, float * b, int dim)
{
	for(int i=0;i<dim;i++)
		a[i]=b[i];
}
void copy_mat(float ** a, float ** b, int dim1, int dim2)
{
	for(int i=0;i<dim1;i++)
		copy_vec(a[i], b[i], dim2);
}
//transpose matrix
void transpose(float ** weights, float ** weights_transpose,int dim1, int dim2)
{
	for(int i=0;i<dim1;i++)
	{
		for(int j=0;j<dim2;j++)
			weights_transpose[j][i]=weights[i][j];
	}
}
// a= a+b;
void add_vector(float * a, float * b, int dim)
{
	for(int i=0;i<dim;i++)
		a[i]+=b[i];
}

void rand_init_vec_int(int * a, int dim,int max_int)
{
	for(int i=0;i<dim;i++)
		a[i]=rand()%max_int;
}
//init vector randomly
void rand_init_vec(float * a, int dim)
{
	for(int i=0;i<dim;i++)
		a[i]=randfloat();
}
//init matrix randomly
void rand_init_mat(float ** a, int dim1, int dim2)
{
	for(int i=0;i<dim1;i++)
		rand_init_vec(a[i], dim2);
}

//save matrix to txt file
void save_mat_txt(char * filename, float ** mat, int rows,int cols)
{
	std::ofstream outfile;
	outfile.open(filename);
	for(int i=0;i<rows;i++)
	{
		for(int j=0;j<cols;j++)
			outfile<<mat[i][j]<<" ";
		outfile<<std::endl;
	}
	outfile.close();
}
//save vector to txt file
void save_vec_txt(char * filename, float *vec, int len)
{
	std::ofstream outfile;
	outfile.open(filename);
	for(int i=0;i<len;i++)
	{
		outfile<<vec[i]<<std::endl;
	}
	outfile.close();
}

float log_sum(float log_a, float log_b)
{
  float v;

  if (log_a < log_b)
  {
      v = log_b+log(1 + exp(log_a-log_b));
  }
  else
  {
      v = log_a+log(1 + exp(log_b-log_a));
  }
  return(v);
}

float log_sum_vec(float * logvec,int D)
{
	float sum=0;
	sum=logvec[0];
	for(int i=1;i<D;i++)
	{
		sum=log_sum(sum,logvec[i]);
	}
	return sum;
}

void log2ori(float * vec,int D )
{
	//save log
	for(int i=0;i<D;i++)
	{
		if(std::abs(vec[i])<1e-10)
			vec[i]=1e-10;
	}
	double lsum=log_sum_vec(vec,D);
	for(int i=0;i<D;i++)
		vec[i]=exp(vec[i]-lsum);
}

//l2 distance
float dis_l2(float *a, float *b,int D)
{
	float sum_sqr=0;
	for(int i=0;i<D;i++)
	{
		float dif=a[i]-b[i];
		sum_sqr=dif*dif;
	}
	return sum_sqr;
}



