
#include <time.h>
#include <stdlib.h>
#include "aux_data.hpp"

void rand_init_vec_int(int * a, int dim,int max_int);
int myrandom2 (int i);
//get time
float get_time();

float randfloat();
//z=x-y
void VecSub(float * x, float * y, float * z, int dim);
//y=mat*x
void MatVecMul(float ** mat, float * x, float * y, int num_rows_mat, int num_cols_mat);
//return ||vec||^{2}
float VecSqr(float * vec,int dim);
//void data sparse format
void LoadSparseData(float ** data, int num_data, const char * file);
//load dense data
void LoadPairs(pair * pairs, int num_pairs, const char * file);
