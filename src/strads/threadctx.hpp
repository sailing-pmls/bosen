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

/********************************************************
  @author: Jin Kyu Kim (jinkyuk@cs.cmu.edu)
  Parallel Lasso problem solver based coordinate descent.

********************************************************/

#if !defined(MAIN_HPP_)
#define MAIN_HPP_

#include "basectx.hpp"
#include "userdefined.hpp"
#include "strads-queue.hpp"
#include "comm_handler.hpp"
#include "spmat.hpp"
#include "dpartitioner.hpp"
#include "interthreadcom.hpp"

#define MAX_COM_GROUP (10)
#define WGR (0)
// workers scheduler group 
#define SGR (1)
// schedulers group 

enum machinetype { mach_appsched, mach_client, mach_sched, mach_unknown }; 
enum ptype { s2c_update, c2s_result, load_dpart, receive_dpart, send_dpart, test, deltaupdate, indsetdispatch, s2c_objective, oocload_dpart, oocload_dpart_done, wake_up, blocking }; 


typedef struct{
  std::map<long unsigned int, dpartitionctx *>thrdwise_map;
  // dpartition id mapped into dpartitionctx - real object 
  std::map<long unsigned int, dpartitionctx *>machwise_map;
}dpartmapctx;

typedef struct{
  int aa;
  /* correlation graph control data structure */
  void *(*sched_master_bg_func)(void *);
}bgschedctx;

typedef struct{
  int aa;
  bgschedctx *bgctx;
  appschedctx *userctx;
}masterctx;

typedef struct{
  // add machine type specific entries 
  void *(*sched_client_bg_func)(void *);
}clientctx;

typedef struct{
    void *(*schedmach_gate_func)(void *); // gate thread on each machine
    experiments *expt;
}schedmachctx;

typedef struct {

  pthread_t pthid;
  int lid;   /* local thread id  */
  int gid;   /* global thread id */
  int rank;  /* MPI machine rank */

  int schedid; 
  int schedgid;
  int schedrank; /* local rank among scheduler machines except for app scheduler machine */

  // in most case of keeping replicas in all machines, 
  // problems ctx would be useful. 
  problemctx *pctx;
  clusterctx *cctx;

  /* user defined application specific context */
  appworker *awctx;
  clientctx *clictx;      /* if the thread is sched-client thread, ctx would be given       */
  masterctx *mastctx;     /* for non sched thread, NULL will be given                       */ 
  schedmachctx *schedmach;/* for app scheduler and worker mach(client), NULL will be given. */

  tqhandler *tqh; /* only for background scheduler */
  wqhandler *wqh; /* only for background scheduler */
  vqhandler *vqh; /* only for background scheduler */

  pthread_barrier_t *allschedbarrier; /* 1 user scheduler threads, n backgroundscheduler threads */
  /* user scheduler(forgreound scheduler) + schedulers in client side are using commhandler */

  problemctx *rawdata;

  //  comctx *ch;
  commtest::comm_handler *comh[MAX_COM_GROUP];
  machinetype machtype;

  // this is for distributed memory support. 
  // map data partition id assigned by the scheduler or app into 
  // data structure used to manage(alloc mem, load data from hdd) the partition. 
  dpartmapctx *dpartmap; // allocate a object using new .. 
  class row_spmat sparsetest;

}threadctx;

//////////////////////////////////////////////////////
// Warning: do not allocate comctx with calloc funcs
//////////////////////////////////////////////////////
typedef struct {

  commtest::comm_handler *handler;
  long rankarray[MAX_SUBSCRIBER];
  long subscribers;
  int serverrank;

}comctx;


typedef struct{

  bool initflag;
  bool dflag;
  long maxmemsize; /* approximate in MB */
  bool invflag;

  void *buf1;  
  void *cmbuf1;
 
  long _buf1_cols;
  long _buf1_rows;
  long _buf1_size; /* approximate in MB */

  void *buf2; 
  void *cmbuf2;

  long _buf2_cols;
  long _buf2_rows;;
  long _buf2_size; /* approximate in MB */

}doublebufctx; /* double buffering context */


// this one is for scheduler thread, not for user thread. 
typedef struct {

  pthread_t pthid;
  int lid;   /* local thread id  */
  int gid;   /* global scheduler thread id */

  int modulo;
  doublebufctx *dbufctx;
  //dpartitionctx *dpartctx; /* use part of dpartition context */

  int rank;  /* MPI machine rank */
  int schedid;

  problemctx *pctx;
  clusterctx *cctx;
  experiments *expt;

  pthread_barrier_t *allschedbarrier; /* 1 user scheduler threads, n backgroundscheduler threads */

  vqhandler *vqh; /* only for background scheduler */

  //long unsigned int maxfeatures_per_thread;
  long unsigned int start_feature;
  long unsigned int end_feature;

  dpartmapctx *dpartmap; // allocate a object using new


  bool xdpready;
  dpartitionctx *xdp;

  pthread_mutex_t mu;
  pthread_cond_t wakeupsignal;  
  int status;


}sghthreadctx;

typedef struct{
  bool bgthreadcreated;
  bool ctxinit;
  pthread_mutex_t mu;
  pthread_cond_t wakeupsignal;

  iothreadq queue;
  iothreadretq retqueue;

  pthread_t bgthid;
  sghthreadctx *refthreadctx; // father's thread context.                                        
  threadctx *mthreadctx; // father's thread context. 
  problemctx *pctx;
  experiments *expt;
  clusterctx *cctx;
  int rank;
  int lid;
  appworker *awctx;

}wbgctx;


typedef struct{

  dpartitionctx *meta_dpart; 
  param_range *selected_prange;
  int selected_prcnt;
  int rank;
  
  commtest::comm_handler *comh;     

}ooccmd_item;


typedef struct{
  dpartitionctx *allocated_dpart;


}oocret_item;


typedef std::deque<ooccmd_item *>ooccmdq;
typedef std::deque<oocret_item *>oocretq;

typedef struct{
  bool pendingio; // checked by thread ctx 
                  // modified by a client thread or a gate thread 

  bool bgthreadcreated;
  bool ctxinit;
  pthread_mutex_t mu;
  pthread_cond_t wakeupsignal;

  ooccmdq cmdq;
  oocretq retq;

  pthread_t bgthid;

  threadctx *mthreadctx; // father's thread context. 
  clusterctx *cctx;
  int rank;
}iothreadctx;

void oocio_init_iothreadctx(iothreadctx *ctx, threadctx *refthreadctx);
void *oocio_thread(void *arg);

void *worker_thread(void *arg);
void init_worker_threadctx(wbgctx *ctx, threadctx *refthreadctx, int lid);

uint8_t *user_client_handler_with_memalloc(threadctx *tctx, wbgctx **childs, clusterctx *cctx, uint64_t *sendmsglen, uint8_t *recvbuf, iothreadctx *ioctx, uint64_t *oocdone);

double multiresidual(double **X, double *Y, double oldB, int idx, int n, int p, double *localRI, double lambda);

void send_dpartition_cmd_to_singlem(threadctx *tctx, char *infn, uint64_t x, uint64_t xlen, uint64_t y, uint64_t ylen, dtype pformat, densitytype sptype, int dst, uint64_t samples, uint64_t features);


void send_dpartition_ooc_cmd_to_singlem(threadctx *tctx, char *infn, uint64_t x, uint64_t xlen, uint64_t y, uint64_t ylen, dtype pformat, densitytype sptype, int dst, uint64_t samples, uint64_t features);

void send_dpartition_cmd_to_schedm(threadctx *tctx, char *infn, uint64_t v_s, uint64_t v_len, uint64_t h_s, uint64_t h_len, dtype pformat, densitytype sptype, int dst, uint64_t samples, uint64_t features);



void send_ooc_dpartition_ooc_cmd_to_singlem(threadctx *tctx, char *infn, uint64_t v_s, uint64_t v_len, uint64_t h_s, uint64_t h_len, dtype pformat, densitytype sptype, int dst, uint64_t samples, uint64_t features, long ooc_hmodflag, long h_modulo, long oocdpartitions, ptype cmdtype);


void send_ooc_dpartition_ooc_cmd_to_schedm(threadctx *tctx, char *infn, uint64_t v_s, uint64_t v_len, uint64_t h_s, uint64_t h_len, dtype pformat, densitytype sptype, int dst, uint64_t samples, uint64_t features, long ooc_hmodflag, long h_modulo, long oocdpartitions, ptype cmdtype);


void init_client_thread(threadctx *tctx, uint64_t samples, uint64_t features);

#endif 
