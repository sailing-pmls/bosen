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
   Distributed scheduler framework 
********************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <sched.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/time.h>
#include <string>
#include <iostream>
#include <pthread.h>
#include <mpi.h>
#include <assert.h>

#include <boost/program_options.hpp>
#include <boost/thread/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/matrix_proxy.hpp>
#include <boost/numeric/ublas/matrix_sparse.hpp>
#include <boost/numeric/ublas/io.hpp>

#include "basectx.hpp"
#include "threadctx.hpp"
#include "options.hpp"
#include "utility.hpp"
#include "iohandler.hpp"
#include "scheduler-bg.hpp"

using namespace std;

static problemctx   pbctx;     /* problem context of the input data set */
static clusterctx   cctx;      /* machine context of application */
static experiments  expt; 
static masterctx    master;
static appschedctx  appsched;
static clientctx    client;    /* workers  */
static schedmachctx schedmach; /* scheduler machines except for app scheduler machine */
static appworker    appworkt;
static bgschedctx   bgmaster;
static numactx      numainfo;
static threadctx    *thctx;    /* three main thread in each machine:
				   - appscheduler will create sched-gate and helper threads if necessary,
				   - client-sched will create worker threads 
				   - sched-gate will reate helper threads */

static void _printcfg(problemctx *pctx, clusterctx *cctx, experiments *expt, appschedctx *app);
static void _create_threadcxt(problemctx *mastercopy, problemctx *pctx, clusterctx *cctx,  int rank, masterctx *master, clientctx *client, appworker *app, commtest::comm_handler *comhandler, commtest::comm_handler *comhandler2, schedmachctx *schedmct, int size);
static commtest::comm_handler * _setup_com_channel(clusterctx *cctx, int myrank, int serverrank, int port_number, int *clientlist, int clientsize);
static void  _init_context(masterctx *master, clientctx *client, appworker *appworkt, bgschedctx *bgmast, appschedctx *appsched, experiments *expt, schedmachctx *schedmctx);
static void _assign_schedmachine(clusterctx *cctx, int rank);
static char *_getipstring(clusterctx *ctx, int rank);
static void _set_parameter_space(problemctx *pctx, uint64_t parameters);
//static void _load_data(problemctx *pctx, int rank);

int main(int argc, char **argv){

  int size, rank, rc, *clientlist;
  pthread_attr_t attr;
  void *res;
  commtest::comm_handler *comm_w = NULL;
  commtest::comm_handler *comm_s = NULL;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  rc = pthread_attr_init(&attr);
  assert(rc==0);

  assert(sizeof(int) == sizeof(uint32_t));
  srand(time(NULL));

  get_options(argc, argv, &pbctx, &cctx, &expt);             // take command line args
  getuserconf(&expt, &cctx, rank);                           // defined by application file 
  iohandler_getsysconf((char *)SYSTEMCONFIG, &numainfo, &pbctx, rank);  // not system.conf file is not used. 
                                                             // system HW information is hard coded.
  cctx.numainfo = &numainfo;
  assert((cctx.wmachines + cctx.schedmachines) == size);  
  
  _set_parameter_space(&pbctx, pbctx.parameters);

  MPI_Barrier(MPI_COMM_WORLD);

  if(rank == 0){
    _printcfg(&pbctx, &cctx, &expt, &appsched);
  }

  assert(pbctx.features < SYSTEM_MACRO_INVALID_INDEX);
  assert(pbctx.samples < SYSTEM_MACRO_INVALID_INDEX);
  /* the number of samples and features 
     should be smaller than INDEX LIMIT */
  MPI_Barrier(MPI_COMM_WORLD);

  _assign_schedmachine(&cctx, rank); 
  _init_context(&master, &client, &appworkt, &bgmaster, &appsched, &expt, &schedmach);

  MPI_Barrier(MPI_COMM_WORLD);
  if(rank < cctx.wmachines || rank == size-1){
    clientlist = (int *)calloc(cctx.wmachines, sizeof(int));
    for(int i=0; i<cctx.wmachines; i++){      
      clientlist[i] = i;
    }
    comm_w = _setup_com_channel(&cctx, rank, size-1, 8000,  clientlist, cctx.wmachines);
  }

  strads_msg(OUT, "[common] machines set up network \n");
  MPI_Barrier(MPI_COMM_WORLD); /* essential barrier don't remove */
  if(rank >= cctx.wmachines && cctx.schedmachines > 1){ 
    clientlist = (int *)calloc(cctx.schedmachines-1, sizeof(int));
    int tmprank=cctx.wmachines;
    for(int i=0; i<cctx.schedmachines-1; i++){
      /* -1 to exclude app scheduler (size-1 rank) */
      clientlist[i] = tmprank;
      tmprank++;
    }
    comm_s = _setup_com_channel(&cctx, rank, size-1, 8002,  clientlist, cctx.schedmachines-1);
    /* -1 to exclude app scheduler (size-1 rank) */
  }

  strads_msg(OUT, "[common] network set up done \n");
  _create_threadcxt(NULL, &pbctx, &cctx,  rank, &master, &client, &appworkt, comm_w, comm_s, &schedmach, size);

  pbctx.prange = (param_range *)calloc((cctx.schedmachines-1)*(cctx.schedthrd_per_mach+1), sizeof(param_range));
  assert(pbctx.prange);
  int progress=0;
  for(int i=0; i<cctx.schedmachines-1; i++){
    uint64_t startc, endc;
    sched_get_my_column_range(i+cctx.wmachines, &cctx, cctx.schedmachines-1, pbctx.features, &startc, &endc);    
    for(int j=0; j<cctx.schedthrd_per_mach; j++){
      uint64_t thrd_startc, thrd_endc;
      _get_thread_col_range(cctx.schedthrd_per_mach, startc, endc, &thrd_startc, &thrd_endc, j);
      pbctx.prange[progress].feature_start = thrd_startc;
      pbctx.prange[progress].feature_end = thrd_endc;
      pbctx.prange[progress].gid = progress;
      progress++;
    }
  }
  pbctx.prangecnt = (cctx.schedmachines-1)*cctx.schedthrd_per_mach; 

  if(rank == 0){
    strads_msg(OUT, "[common] worker mcachines(%d) schedmachine(%d) mpi size(%d)\n", 
	       cctx.wmachines, cctx.schedmachines, size);
  }

  if(rank == 0){
    for(int i=0; i<cctx.schedmachines-1; i++){
      for(int j=0; j<cctx.schedthrd_per_mach; j++){   
	uint64_t idx=i*cctx.schedthrd_per_mach + j;
	strads_msg(OUT, "\tscheduler parameter assign : sched_mach(%d) lid(%d) gid(%ld) from (%ld) to (%ld) \n", 
		   i, j,idx, pbctx.prange[idx].feature_start, pbctx.prange[idx].feature_end);
      }
    }
    strads_msg(ERR, "Sched ParamRangeCNT : %d\n", 
	       pbctx.prangecnt);
  }

  MPI_Barrier(MPI_COMM_WORLD); /* essential barrier don't remove */
  if(thctx->machtype == mach_appsched){
    rc = pthread_create(&thctx->pthid, &attr, thctx->mastctx->userctx->user_scheduler_func, thctx);
  }
  if(thctx->machtype == mach_client){
    rc = pthread_create(&thctx->pthid, &attr, thctx->clictx->sched_client_bg_func, thctx);
  }
  if(thctx->machtype == mach_sched){
    strads_msg(ERR, "*** rank %d create mach_sched\n", rank);
    rc = pthread_create(&thctx->pthid, &attr, thctx->schedmach->schedmach_gate_func, thctx);
    assert(rc == 0);
  }

  /* block the main.cpp thread */
  rc = pthread_join(thctx->pthid, &res);
  assert(rc == 0);

  MPI_Barrier(MPI_COMM_WORLD);
  strads_msg(ERR, "[main.cpp] machine %d leaving.\n", rank);
  MPI_Finalize();

  return 0;
}

bool checkmyrank(int myrank, int *clientlist, int clientsize){

  bool retb = false;
  strads_msg(INF, "check myrank: myrank[%d]\n", myrank);
  for(int i=0; i<clientsize; i++){
    if(myrank == clientlist[i]){
      retb = true;
      break;
    }
  }

  return retb;
}


commtest::comm_handler * _setup_com_channel(clusterctx *cctx, int myrank, int serverrank, int port_number, int *clientlist, int clientsize){

  if(checkmyrank(myrank, clientlist, clientsize)){
        sleep((myrank+1));    
  }

  strads_msg(ERR, "\t set up com channel myRANK(%d) port(%d)\n", myrank, port_number);
  char portchar[64];
  memset(portchar, 0x0, 64);
  char *cip = _getipstring(cctx, myrank);
  std::string ip(cip);

  //  cout << "cout my rank: " << myrank << " my IP: " << ip << endl;
  cip = _getipstring(cctx, serverrank); /* get server's address */
  std::string sip(cip);
  //  cout << "cout my rank: " << myrank << " server IP: " << sip << endl;
  sprintf(portchar, "%d", port_number);
  strads_msg(INF, "\t\t : %s\n", portchar);  
  std::string port(portchar);

  commtest::cliid_t sid = serverrank;
  commtest::cliid_t id = myrank;

  commtest::cliid_t *clients;
  //  cout << "IP SIP PORT SID ID " << endl;
  cout << ip << sip << endl;
  strads_msg(INF, "sid: %d  id: %d \n", sid, id);

  commtest::comm_handler *comm = NULL;
  if(myrank == serverrank){

    strads_msg(ERR, "I am Server Rank \n");
    commtest::config_param_t config(id, true, ip, port);
    try{
      clients = new commtest::cliid_t[clientsize-1];
    }catch(std::bad_alloc e){
      strads_msg(ERR, "memory alloc failed for clients array \n");
      return NULL;
    }
    try{
      comm = new commtest::comm_handler(config);
    }catch(std::bad_alloc e){
      strads_msg(ERR, "failed to create commhandler \n");
      return NULL;
    }
    strads_msg(ERR, "commhandler is created \n"); 
    for(int i=0; i < clientsize; i++){
      commtest::cliid_t cid;
      strads_msg(ERR, "server: waiting for client %d\n", i);
      cid = comm->get_one_connection();
      if(cid < 0){
	strads_msg(ERR, "wait for connection failed \n");
	return NULL;
      }
      strads_msg(ERR, "\t\treceived connection request form node %d \n", cid);
      clients[i] = cid;
    }
  }
  
  if(checkmyrank(myrank, clientlist, clientsize)){
    strads_msg(ERR, "\t\t Worker rank (%d) ip(%s -len(%ld)) send signal to sip(%s - len(%ld))\n", 
	       myrank, ip.c_str(), strlen(ip.c_str()), sip.c_str(), strlen(sip.c_str()));

    commtest::config_param_t config(id, true, ip, port);
    try{
      comm = new commtest::comm_handler(config);
    }catch(std::bad_alloc e){
      strads_msg(ERR, "failed to create commhandler \n");
      return NULL;
    }
    strads_msg(INF, "\t\t client or scheduler machine %d sip:%s sid:%d port:%s \n", 
	       id, sip.c_str(), sid, port.c_str());
    comm->connect_to(sip, port, sid); 
  }
  strads_msg(ERR, "\t\t @@@@@ mpi rank(%d) \n", myrank);
  return comm;
}

static void _create_threadcxt(problemctx *mastercopy, problemctx *pctx, clusterctx *cctx,  int rank, masterctx *master, clientctx *client, appworker *app, commtest::comm_handler *wgr, commtest::comm_handler *sgr, schedmachctx *schedmct, int size){

  int appschedrank = cctx->schedmachineranks[cctx->schedmachines-1];
  int firstschedrank = cctx->schedmachineranks[0];
  thctx = (threadctx *)calloc(1, sizeof(threadctx));
  assert(thctx != NULL);

  thctx->lid = -1; /* only for local machine's thread id */
  thctx->gid = -1;
  thctx->rank = rank;

  thctx->schedid = -1;
  thctx->schedgid = -1;
  thctx->schedrank = -1;

  thctx->cctx = cctx;
  thctx->pctx = pctx;
  thctx->mastctx = master;      
  thctx->comh[WGR] = wgr;
  thctx->rawdata = mastercopy;

  thctx->awctx = NULL;
  thctx->clictx = NULL;
  thctx->schedmach = NULL;
  thctx->mastctx = NULL; 
  thctx->machtype = mach_unknown;

  if(rank < firstschedrank){ /* worker machines */
    thctx->machtype = mach_client; 
    thctx->clictx = client;
    thctx->awctx = app;
    thctx->lid = cctx->worker_per_mach; /* 0 ~ n-1 : assigned to worker threads, 
					   n is assigned to scheduler client */
    strads_msg(ERR, "\t\t rank(%d) worker machine\n", rank);
  }

  if(appschedrank == rank){ /* app scheduler machine */
    thctx->machtype = mach_appsched;  
    thctx->mastctx = master;
    thctx->schedrank = rank; 
    thctx->comh[SGR] = sgr;
    strads_msg(ERR, "\t\t rank(%d) app scheduler\n", rank);
  }

  if((firstschedrank <= rank) && (rank < appschedrank)){ // scheduler machines 
    thctx->machtype = mach_sched;  
    thctx->schedmach = schedmct;
    thctx->schedrank = rank; // -1 for counting application scheduler
    thctx->comh[SGR] = sgr;
    strads_msg(ERR, "\t\t rank(%d) scheduler machine\n", rank);
    thctx->schedid = rank - firstschedrank;/* machine id among scheduler machines (0 ~ schedmachines-1) */
  }

  assert(thctx->machtype != mach_unknown);
  return;
}

void _printcfg(problemctx *pctx, clusterctx *cctx, experiments *expt, appschedctx *app){

  strads_msg(ERR, "---------------------------- Problem Set ----------------------------\n");
  strads_msg(ERR, "file-x: %s \nfile-y: %s\n", pctx->xfile, pctx->yfile);
  strads_msg(ERR, "samples: %14ld \t features: %14ld\n\n", pctx->samples, pctx->features);
  strads_msg(ERR, "standardization flag: %ld \t cmflag: %ld\n", pctx->stdflag, pctx->cmflag);

  strads_msg(ERR, "---------------------- Cluster Configuration  -----------------------\n"); 
  strads_msg(ERR, "total-machine: %6d  (wmachine: %6d  schedmachine: %6d\n", 
	  cctx->totalmachines, cctx->wmachines, cctx->schedmachines );
  strads_msg(ERR, "total worker threads: %8d \t worker per mach: %8d\n", 
	     cctx->totalworkers, cctx->worker_per_mach);
  strads_msg(ERR, "total scheduler threads: %5d \t schedthrd per mach: %5d\n\n", 
	     cctx->totalschedthrd, cctx->schedthrd_per_mach);

  strads_msg(ERR, "----- Application Specific Configuration (print user config) -------\n"); 

  user_printappcfg(expt);
  strads_msg(ERR, "--------------------------------------------------------------------\n\n");
}

void  _init_context(masterctx *master, clientctx *client, appworker *appworkt, bgschedctx *bgmast, appschedctx *appsched, experiments *expt, schedmachctx *schedmctx){
  master->bgctx = bgmast;
  //  master->bgctx->sched_master_bg_func = &scheduler_master_background;
  /* TODO if bgschedctx gets more entires, fill here */ 
  master->userctx = appsched;
  master->userctx->user_scheduler_func = &userscheduler;
  /* TODO if appschedctx gets more entires, fill here */ 
  client->sched_client_bg_func = &scheduler_client_machine;
  schedmctx->schedmach_gate_func = &scheduler_gate_thread;
  schedmctx->expt = expt;

  appworkt->user_worker_func = NULL;
  appworkt->expt = expt;
  appsched->expt = expt;
}

void _assign_schedmachine(clusterctx *cctx, int rank){
  int appschedrank = cctx->totalmachines - cctx->schedmachines;
  assert(cctx->schedmachines <= MAX_SCHED_MACH);
  assert(1 <= cctx->schedmachines);
  for(int i=0; i<cctx->schedmachines; i++){
    cctx->schedmachineranks[i] = appschedrank + i;
  }

  if(rank == 0){
    for(int i=0; i<cctx->schedmachines; i++){
      strads_msg(ERR, " %d-th scheduler machine rank: %d \n", i, cctx->schedmachineranks[i]);
    }
  }
}

/* return an IP correspond to my rank */
char *_getipstring(clusterctx *ctx, int rank){
  char *tmpip, *ret;
  tmpip = (char *)calloc(64, sizeof(char));
  assert(tmpip != NULL);
  assert(ctx->ipfile != NULL);
  strads_msg(INF, "ip file name : %s \n", cctx.ipfile);
  FILE *fp = fopen(ctx->ipfile, "r");
  assert(fp != NULL);
  for(int i=0; i<rank+1; i++){
    ret = fgets(tmpip, 64, fp);
    assert(ret != NULL);
  }  
  strads_msg(INF, "machine rank: %d ip:%s\n", rank, tmpip);

  int len = strlen(tmpip);
  if(tmpip[len-1] == '\n'){
    tmpip[len-1] = '\0';
  }

  return tmpip;
}

void _set_parameter_space(problemctx *pctx, uint64_t parametercnt){
  pctx->features = parametercnt;
}

#if 0 
void _load_data(problemctx *pctx, int rank){
  long features, samples;
  iohandler_sf_count(pctx->xfile, &samples, &features);
  iohandler_allocmem_problemctx(pctx, samples, features, pctx->cmflag);
  assert(samples > 0);
  assert(features > 0);
  strads_msg(ERR, "Mach(%d) start loading X with %ld samples and %ld features\n", 
	     rank, samples, features);
  
  uint64_t *randomcols = (uint64_t *)calloc(features, sizeof(uint64_t));
  for(long i=0; i < features; i++){
    randomcols[i] = i;    
  }
  permute_globalfixed_list(randomcols, features);
  pctx->random_columns = randomcols;
  strads_msg(ERR, "Rank(%d) got randomcols (0:%ld, 1:%ld, 2:%ld ....n-2:%ld  n-1:%ld, pctx->random_columns(%p))\n", 
	     rank, randomcols[0], randomcols[1], randomcols[2], randomcols[features-2], 
	     randomcols[features-1], pctx->random_columns); 

  iohandler_read_fulldata(pctx, rank, pctx->stdflag, randomcols);
  strads_msg(ERR, "Mach(%d) start column major organization\n", rank);
  std::srand(time(NULL));
  iohandler_makecm(pctx);	
  /* if cmflag were set to 1, allocmem allocate memory for column majored ones */ 
}
#endif 
