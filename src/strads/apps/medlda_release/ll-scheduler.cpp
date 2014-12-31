#include <stdio.h>
#include <strads/include/common.hpp>
#include <strads/util/utility.hpp>
#include <iostream>     // std::cout
#include <algorithm>    // std::for_each
#include <vector>       // std::vector
#include <unordered_map>
#include <glog/logging.h>
#include <mpi.h>
#include <strads/ps/strads-ps.hpp>
#include "medlda-ps.hpp"

using namespace std;

void *scheduler_mach(void *arg){
  sharedctx *ctx = (sharedctx *)arg;
  strads_msg(ERR, "STRADS SCHEDULER rank %d \n", ctx->rank);
  psbgthreadctx *bctx = (psbgthreadctx *)calloc(sizeof(psbgthreadctx), 1);
  bctx->parentctx = ctx;

  pthread_attr_t m_attr;
  pthread_t m_pthid;
  int rc = pthread_attr_init(&m_attr);

  unordered_map<string, string>*table = new unordered_map<string, string>;  

  checkResults("pthread attr init m_attr failed", rc);
  rc = pthread_create(&m_pthid, &m_attr, ps_server_recvthread, (void *)bctx);
  checkResults("pthread create failed in scheduler_threadctx", rc);

  init_ps(ctx, 
	  NULL, 
	  server_pgasync, 
	  server_putsync, 
	  server_getsync,
	  (void *)table);

  strads_msg(ERR, "[scheduler] ready for exit  remove infinite loop %d \n", ctx->rank);
  while(1); 
  return NULL;
}
