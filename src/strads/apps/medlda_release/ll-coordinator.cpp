#include <stdio.h>
#include <strads/include/common.hpp>
#include <strads/include/child-thread.hpp>
#include <strads/util/utility.hpp>
#include <iostream>     // std::cout
#include <algorithm>    // std::for_each
#include <vector>       // std::vector
#include <glog/logging.h>
#include <mpi.h>
#include "ldall.hpp"

using namespace std;

void make_iteration(sharedctx *ctx){
  for(int i=0; i<ctx->m_worker_machines; i++){
    int length=-1;
    void *buf = ctx->sync_recv(src_worker, i, &length);
    assert(length > 0);
    strads_msg(ERR, "[coordinator] got sync msg from the worker (%d) ptr %p \n", i, buf);
  }
  strads_msg(INF, "[coordinator]  Got signal from  all workers \n");
  int *go = (int *)calloc(sizeof(int), 1);
  *go = 1000;
  for(int i=0; i<ctx->m_worker_machines; i++){
    ctx->send((char *)go, sizeof(int), dst_worker, i); // send one msg to worker i                                          
  }
}

void *coordinator_mach(void *arg){
  sharedctx *ctx = (sharedctx *)arg;
  strads_msg(ERR, "@@@@@ STRADS COORDINATOR rank %d  mworkers: %d \n", ctx->rank, ctx->m_worker_machines);
  strads_msg(ERR, "[coordinator ]  EXIT program normally .. remove infinite loop \n");  

  make_iteration(ctx); // first barrier between phase1 and phase2
  make_iteration(ctx); // second barrier after training ends

  while(1);
  return NULL;
}
