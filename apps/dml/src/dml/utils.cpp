
#include "utils.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>


void rand_init_vec_int(int * a, int dim,int max_int)
{
  for(int i=0;i<dim;i++)
    a[i]=rand()%max_int;
}


int myrandom2(int i) {
  return std::rand()%i;
}


// get time
float get_time() {
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC, &start);
  return (start.tv_sec + start.tv_nsec/1000000000.0);
}

float randfloat() {
  int prec = 10000;
  int rint = rand()%prec;
  float rv = (rint*2-prec)*1.0/prec/100;
  return rv;
}
// z=x-y
void VecSub(float * x, float * y, float * z, int dim) {
  for (int i = 0; i < dim; i++)
    z[i] = x[i]-y[i];
}
// y=mat*x
void MatVecMul(float ** mat, float * x, float * y, \
    int num_rows_mat, int num_cols_mat) {
  for (int i = 0; i < num_rows_mat; i++) {
    float sum = 0;
    for (int j = 0; j < num_cols_mat; j++) {
      sum += mat[i][j]*x[j];
    }
    y[i] = sum;
  }
}
// return ||vec||^{2}
float VecSqr(float * vec, int dim) {
  float sqr = 0;
  for (int i = 0; i < dim; i++)
    sqr += vec[i]*vec[i];
  return sqr;
}
// void data sparse format
void LoadSparseData(float ** data, int num_data, const char * file) {
  FILE * fp = fopen(file, "r");
  int num_tokens = 0, idx = -1;
  float value;
  for (int i = 0; i < num_data; i++) {
    fscanf(fp, "%d", &num_tokens);
      fscanf(fp, "%d", &num_tokens);

    for (int j = 0; j < num_tokens; j++) {
      fscanf(fp, "%d:%f", &idx, &value);
      data[i][idx] = value;
    }
  }
  fclose(fp);
}
// load dense data
void LoadPairs(pair * pairs, int num_pairs, const char * file) {
  std::ifstream infile;
  infile.open(file);
  for (int i = 0; i < num_pairs; i++) {
    infile >> pairs[i].x >> pairs[i].y;
  }
  infile.close();
}
