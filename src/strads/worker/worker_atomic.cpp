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
/***********************************************************
   @author: Jin Kyu Kim (jinkyuk@cs.cmu.edu)
   @project: STRADS: A ML Distributed Scheduler Framework 
***********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/time.h>
#include <string>
#include <iostream>
#include <pthread.h>
#include <mpi.h>
#include <assert.h>
#include "./include/strads-macro.hpp"
#include "./include/common.hpp"
#include "com/comm.hpp"
#include "ds/dshard.hpp"
#include "ds/binaryio.hpp"
#include "com/zmq/zmq-common.hpp"
#include "worker/worker.hpp"
#include <glog/logging.h>

#include "./include/messageclass.hpp"
#include "./include/vuserfuncs-common.hpp"

using namespace std;

pthread_barrier_t   barrier; // barrier synchronization object

void *worker_mach(void *arg){

  sharedctx *ctx = (sharedctx *)arg;
  strads_msg(ERR, "[worker-machine] rank(%d) boot up worker-mach (%d) \n", ctx->rank, ctx->m_worker_mid);
  assert(ctx->m_worker_mid >=0);
  int thrds = ctx->m_params->m_sp->m_thrds_per_worker;  
  pthread_barrier_init (&barrier, NULL, thrds);
  strads_msg(ERR, "[worker] rank(%d) worker_mid(%d) has recvport(%lu) sendport(%lu)\n",
	     ctx->rank, ctx->m_worker_mid, ctx->star_recvportmap.size(), ctx->star_sendportmap.size()); 

  sleep(5); // wait for all worker threads to be created 
  strads_msg(ERR, "\t\t Rank(%d) got (%ld) free threads (%d) remove infinit loop\n", 
	     ctx->rank, ctx->get_freethrdscnt(), ctx->rank);

  assert(ctx->star_recvportmap.size() == 1);
  assert(ctx->star_sendportmap.size() == 1);
  auto pr = ctx->star_recvportmap.begin();
  _ringport *rport = pr->second;
  context *recv_ctx = rport->ctx;
  auto ps = ctx->star_sendportmap.begin();
  _ringport *sport = ps->second;
  context *send_ctx = sport->ctx;

  timers timer(10, 10);
  bool tflag = false;
  while (1){
    void *msg = recv_ctx->pull_entry_inq();
    if(msg != NULL){
      mbuffer *mbuf = (mbuffer *)msg;     
      tflag = false;
      mbuf = (mbuffer *)process_common_system_cmd(ctx, mbuf, recv_ctx, send_ctx, &tflag);  // done by machine agent (worker_mach)
      if(tflag == true){
	strads_msg(ERR, "SYSTEM DSHARD COMMAND IS DONE \n");
	break;
      }     
    }
  }

  DECLARE_FUNCTION(ctx)
  pretmsg = NULL;

  while (1){

    void *msg = recv_ctx->pull_entry_inq();
    if(msg != NULL){
      mbuffer *mbuf = (mbuffer *)msg;     
      message_type msgtype = mbuf->msg_type;
      if(msgtype == USER_PROGRESS_CHECK){
	
	usermsg_one<uload<result>> *pobj;
	pobj = (usermsg_one<uload<result>> *)mbuf->data;
	pobj->deserialize(mbuf->data, -1);

	handler->do_obj_calc(*pobj, *ctx);
	mbuffer *sndbuf = (mbuffer *)calloc(sizeof(mbuffer), 1);
	sndbuf->msg_type = USER_PROGRESS_CHECK;
	char *tmp = pobj->serialize();      
	memcpy(sndbuf->data, tmp, pobj->get_blen());
	
	while(send_ctx->push_entry_outq(sndbuf, sizeof(mbuffer)));     
	continue;
      }

      msgtype = mbuf->msg_type;
      assert(msgtype == USER_UPDATE);

      usermsg_two<uload<task>, uload<status>> *pmsg2;

      pmsg2 = (usermsg_two<uload<task>, uload<status>> *)mbuf->data;
      pmsg2->deserialize(mbuf->data, -1); // remove -1 araument later 

      usermsg_one<uload<result>> resmsg(USER_UW_RES, pmsg2->umember1.size_);

      handler->update_status(*pmsg2, *ctx);
      handler->do_work(*pmsg2, *ctx, resmsg);    

      mbuffer *rbuffer = (mbuffer *)calloc(1, sizeof(mbuffer));      
      assert(rbuffer);

      char *tmp = resmsg.serialize();

      rbuffer->src_rank = ctx->rank;
      rbuffer->msg_type = msgtype;

      assert(resmsg.get_blen() + 64 < (long)sizeof(mbuffer));
      memcpy(rbuffer->data, tmp, resmsg.get_blen());

      while(send_ctx->push_entry_outq(rbuffer, sizeof(mbuffer)));
      recv_ctx->release_buffer((void *)mbuf); // don't forget this           
    }// if (msg != NULL) ..
  } // end of while(1)
  return NULL;
}
