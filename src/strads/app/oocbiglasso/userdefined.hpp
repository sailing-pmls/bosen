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
#if !defined(USER_DEFINED_HPP)
#define USER_DEFINED_HPP

#include <map>
#include <iostream>
#include "basectx.hpp"
#include "threadctx.hpp"
#include "dpartitioner.hpp"

using namespace std;

#define MAX_BETA_TO_UPDATE (1024*1024)
#define BIGLASSO_INVALID_VID 0xFFFFFFFFFFFFFFFF

typedef struct{
  uint64_t vid;
  double  beta;
  double oldbeta;
}idval_pair;

typedef struct{
  uint64_t vid;
  double mpartial;
  double xsqmsum;

}idmpartial_pair;

typedef struct{
  uint64_t betacnt;
  idval_pair id_val_pair[MAX_BETA_TO_UPDATE];
}prev_beta;


typedef struct{
  int      vid;
  double   beta;
}vbpair;

typedef struct{
  std::map<int, double>betamap;
}dynamic_sp_beta;

/* Application specific data structure *********************/
typedef struct{

  int maxiter;       /* the number of operation dispatch */
  double timelimit;     /* seconds */

  double objthreshold;
  char *userconfigfile;

  double lambda;
  double alpha;
  double rho;

  int dfrequency;
  int batchsize;
  int poolsize;

  int desired; // not from user input. calculated based on alpha 
  int afterdesired; // not from user input. calculated based on alpha 

  char *outputdir; // user input 

  double penaltyweight;
  double sampleratio;
  int samplingsize;

  long unsigned int scanwindow;
  double initminunit;

  double dpercentile;

  long oocfreq;

  long oocpartitions;
  double init_sweepoutrate;

}experiments;

/* lasso example */
typedef struct{
  double *beta;
  double *oldbeta;
  double *residual;
  long unsigned int *vidxlist;
  experiments *expt;
  void *(*user_scheduler_func)(void *);
}appschedctx;

typedef struct{
  experiments *expt;  
  void *(*user_worker_func)(void *);

  double *beta;

  uint64_t samples;
  uint64_t features;

  uint64_t dpartslot;
  dpartitionctx *dpart[100];

  rangectx thrange[128];

  double *residual;
  double *yvector;

  // for further fine grained partitioning. 

}appworker;





void getuserconf(experiments *, clusterctx *cctx, int rank);
void user_printappcfg(experiments*app);
void *userscheduler(void *arg);
void *user_app_worker(void *arg);

void biglasso_update_residual(dpartitionctx *dp, double *beta, double *oldbeta, double *residual, uint64_t vid);

void lasso_read_y(uint64_t rank, char *yfn, uint64_t samples, double *vec);

/* end of application specific data structure declaration */
#endif
