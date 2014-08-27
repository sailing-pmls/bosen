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
/*********************************************************************************

   @author: Jin Kyu Kim (jinkyuk@cs.cmu.edu)
   @project: STRADS: A ML Distributed Scheduler Framework 
   @title: common cmd handler for schedulers / workers 
           This file contains common command handler across scheduler / worker. 
           On receiving cmd from coorodinator, worker / scheduler will call
           routines here 
           Scheduler / worker specific routines are placed in 
           ./scheduler ./worker directories. 

*********************************************************************************/

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
#include "ds/dshard.hpp"
#include "ds/binaryio.hpp"
#include "com/comm.hpp"
#include "com/zmq/zmq-common.hpp"
#include "worker/worker.hpp"
#include <glog/logging.h>
#include "./include/indepds.hpp"


using namespace std;

void mcopy_broadcast_to_workers(sharedctx *ctx, void *tmp, int len){
  mbuffer *sbuffer = (mbuffer *)calloc(1, sizeof(mbuffer));
  sbuffer->msg_type = USER_UPDATE;
  memcpy(sbuffer->data, tmp, len);
  for(int i=0; i < ctx->m_worker_machines; i++){
    mbuffer *tmp = (mbuffer *)calloc(1, sizeof(mbuffer));
    memcpy(tmp, sbuffer, sizeof(mbuffer));
    while(ctx->worker_sendportmap[i]->ctx->push_entry_outq((void *)tmp, sizeof(mbuffer)));
  }
  free(sbuffer);
}

void mcopy_broadcast_to_workers_objectcalc(sharedctx *ctx, void *tmp, int len){
  mbuffer *sbuffer = (mbuffer *)calloc(1, sizeof(mbuffer));
  sbuffer->msg_type = USER_PROGRESS_CHECK;
  memcpy(sbuffer->data, tmp, len);
  for(int i=0; i < ctx->m_worker_machines; i++){
    mbuffer *tmp = (mbuffer *)calloc(1, sizeof(mbuffer));
    memcpy(tmp, sbuffer, sizeof(mbuffer));
    while(ctx->worker_sendportmap[i]->ctx->push_entry_outq((void *)tmp, sizeof(mbuffer)));
  }
  free(sbuffer);
}

void *process_common_system_cmd(sharedctx *ctx, mbuffer *mbuf, context *recv_ctx, context *send_ctx, bool *tflag){ 
  // don't forget release call on mbuf  

  if(mbuf == NULL)
    assert(0);

  if(mbuf->msg_type == SYSTEM_DSHARD_END){
    long cmdid = mbuf->cmdid;
    recv_ctx->release_buffer((void *)mbuf); // don't forget this 
    mbuffer *mtmp = (mbuffer *)calloc(1, sizeof(mbuffer));
    testpkt *stpkt = (testpkt *)mtmp->data;
    mtmp->cmdid = cmdid;
    mtmp->src_rank = ctx->rank;
    stpkt->seqno = 0;
    *tflag = true;
    while(send_ctx->push_entry_outq(mtmp, sizeof(mbuffer)));
    return (void *)mbuf; // if I can not handle this, pass it to the next processor
  }

  if(mbuf->msg_type == SYSTEM_DSHARD){
    long cmdid = mbuf->cmdid;
    sys_packet *spacket = (sys_packet *)mbuf->data;      
    mini_dshardctx *mshard = (mini_dshardctx *)spacket->syscmd;      

    dshardctx *dshard = new dshardctx(*(mshard));

    if(dshard->m_type == d2dmat){

      dshard->m_dmat.resize(dshard->m_rows, dshard->m_cols);
#if defined(TEST_ATOMIC)
      dshard->m_atomic.resize(dshard->m_rows);
#endif 
      strads_msg(ERR, "for dense matrix, call resize \n");
    }
    strads_msg(OUT, "Rank(%d) DSHARD ptr (%p) : ALIAS(%s) \n", ctx->rank, dshard, dshard->m_alias);
    assert(dshard != NULL);
    ctx->register_shard(dshard);   
    strads_msg(ERR, "Rank(%d) dshard->range->c_start (%ld) end(%ld)\n",
	       ctx->rank, dshard->m_range.c_start, dshard->m_range.c_end); 
    strads_msg(ERR, "Rank(%d) dshard->range->r_start (%ld) end(%ld)\n",
	       ctx->rank, dshard->m_range.r_start, dshard->m_range.r_end); 

    //    if(strcmp(dshard->m_fn, "empty") != 0){
    if(strcmp(dshard->m_fn, IN_MEMORY_DS_STRING ) != 0){
      // in memory data struecture has no file to upload. in that case, input file name should be "empty"
#if !defined(COL_SORT_INPUT)
      iohandler_spmat_pbfio_partialread(dshard, false, ctx->rank, ctx);
#else

      if(ctx->m_mrole == mrole_scheduler and strcmp(dshard->m_alias, "Acol") == 0 ){
	assert(dshard->m_type == cvspt);
	strads_msg(ERR, "Rank (%d) mach (scheduler) load Acol matrix using partialread_sorted order (rowsort..) \n", 
		   ctx->rank);
	iohandler_spmat_pbfio_partialread_sortedorder(dshard, false, ctx->rank, rowsort, ctx);	       
	strads_msg(ERR, "SCHEDULER RANK(%d) load %ld nz entry from input file \n", 
		   ctx->rank, dshard->m_cm_vmat.allocatedentry());      
      }else{
	iohandler_spmat_pbfio_partialread(dshard, false, ctx->rank, ctx);
      }
#endif      
    }else{
      // instead of loading data from the file, reset the array with zero.
#if defined(TEST_ATOMIC)
      dshard->m_atomic.zero_fill(dshard->m_rows);
#endif 
      strads_msg(ERR, "No input file is associated with this data strcuture\n");
    }

#if 1
    // bind distirbuted shard with parameters of user func 
    for(auto p : ctx->m_params->m_sp->m_funcmap){
      userfunc_entity *ent = p.second;
      strads_msg(ERR, " (%d) rank (%d) func name %s  paramcnt %d \n", 
		 ctx->rank, p.first, ent->m_strfuncname.c_str(), ent->m_param_shards);     
    }
    strads_msg(OUT, "Rank(%d) conducting function parameter binding \n", ctx->rank);    
    ctx->m_params->m_sp->bind_func_param(dshard->m_alias, (void *)dshard);
#endif

    recv_ctx->release_buffer((void *)mbuf); // don't forget this 

    mbuffer *mtmp = (mbuffer *)calloc(1, sizeof(mbuffer));
    testpkt *stpkt = (testpkt *)mtmp->data;
    mtmp->cmdid = cmdid;
    mtmp->src_rank = ctx->rank;
    stpkt->seqno = 0;
    while(send_ctx->push_entry_outq(mtmp, sizeof(mbuffer)));
    return NULL;     
  }
  return (void *)mbuf; // if I can not handle this, pass it to the next processor
}
