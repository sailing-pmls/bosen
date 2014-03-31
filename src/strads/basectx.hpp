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

/*******************************************************************
  @author: Jin Kyu Kim (jinkyuk@cs.cmu.edu)

  @desc: The header file contains base data structure for 
         problem context, machine environment context, and scheduling context.

********************************************************************/

#if !defined(BASECTX_H_)
#define BASECTX_H_

#include <pthread.h>
#include "systemmacro.hpp"

enum mattype { rmajor, cmajor }; 
/* cmajor: column major matrix 
   rmajor: row major */

typedef struct{

  int socket;
  int corepersocket;
  int totalcores;

  int membanks;
  long totalmemsize; /* in KB */
  long banksize;     /* in KB */

  int l1cache;       /* in KB */
  int l2cache;       /* in KB */
  int l3cache;       /* in KB */

}numactx;

typedef struct{
  int mrank;
  int lid;
  int gid;

  long unsigned int feature_start;
  long unsigned int feature_end;
}param_range;

/*********************************************************
 * problem configuration
*********************************************************/
typedef struct{

  char *xfile;   /* file name of X      */
  char *yfile;   /* file name of Y */

  double **X;    /* input matrix X       */
  double *Y;     /* observation matrix Y */

  long features;  /* the number of features (the number of columns of X) */
  long samples;   /* the number of samples  (the number of rows of X )   */
  long allocatedfeatures;
  long allocatedsamples;

  long stdflag;     /* 1: input are standardized, 0: otherwise */
  long cmflag;

  double *xsum;    /* if standardized, it can be simply set to 0, 
		      otherwise, set right value to this field */

  double *xsqsum;  /* if standardized, it can be simply set to 1, 
		      otherwise, set right value to this field */

  double **cmX;    /* for column access, make column major matrix */
                   /* xsum, xsqsum, cmX are used for correlation calculation */

  double rhothreshold; /* threshold of correlation : since the framework support correlation aware scheduling 
			  as a primitive feauture, it should support correlation threshold as a basic parameter */

  double *pxsum;   /* array with features elements */
  double *pxsqsum; /* array with features elements */

  long unsigned int *random_columns;

  long unsigned int parameters;
  long unsigned int roundsize;


  int prangecnt;
  param_range *prange;

}problemctx;

/*********************************************************
 * context of cluster machines 
*********************************************************/
typedef struct{
  int totalmachines;

  int wmachines;            /* the number of machines */
  int worker_per_mach;      /* the number of thread per machine */
  int totalworkers;         /* the number of total threads */

  int schedmachines;
  int schedthrd_per_mach;
  int totalschedthrd;

  int size;          /* real number of machines including scheduler(s) */
  int schedrank;     /* scheduler's rank */

  numactx *numainfo; /* for stage 3 */

  char *ipfile; /* machine file in MPI or a file of IP addresses in the cluster */

  int schedmachineranks[MAX_SCHED_MACH];

}clusterctx;



typedef struct{
  long nzcnt;
  long start_idx;
}columnbank;
typedef struct{
  long rowidx;
  double value;  
}colarray;
typedef struct{
  bool active;
  long start_col;
  long end_col;
  long start_row;
  long end_row;

  long colsize;
  columnbank *cbank;  

  long array_alloc;
  colarray *array;
}sp_colmat;



typedef struct{
  long nzcnt;
  long start_idx;
}rowbank;
typedef struct{
  long colidx;
  double value;  
}rowarray;
typedef struct{
  bool active;
  long start_col;
  long end_col;
  long start_row;
  long end_row;

  long rowsize;
  rowbank *rbank;  

  long array_alloc;
  rowarray *array;
}sp_rowmat;


typedef struct{
  long nzcnt;
  long start_idx;
}spbank;
typedef struct{
  long idx;
  double value;  
}sparray;
typedef struct{
  bool active;
  mattype type;
  long start_col;
  long end_col;
  long start_row;
  long end_row;

  long banksize;
  spbank *bank;  

  long array_alloc;
  sparray *array;
}sp_mat;

#endif
