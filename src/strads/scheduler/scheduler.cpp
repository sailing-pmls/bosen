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
#include "ds/dshard.hpp"
#include "ds/binaryio.hpp"
#include "com/comm.hpp"
#include "com/zmq/zmq-common.hpp"
#include "scheduler/scheduler.hpp"
#include <glog/logging.h>
#include "./include/indepds.hpp"

#include "./include/messageclass.hpp"
#include "./include/vuserfuncs-common.hpp"

using namespace std;

void *process_sched_system_cmd(sharedctx *ctx, mbuffer *mbuf, scheduler_threadctx **sthrds, context *recv_ctx, context *send_ctx){

  if(mbuf == NULL)
    return NULL; // since previous handler processes that and set mbuf to NULL
  
  if(mbuf->msg_type == SYSTEM_SCHEDULING){  

    schedhead *schedhp = (schedhead *)mbuf->data; // mbuf passed from caller, should be freed in the caller 
                                                  // do not call free here
    if(schedhp->type == SCHED_START){
      sched_start_p *amo = (sched_start_p *)((uintptr_t)schedhp + sizeof(schedhead));
      int64_t taskcnt = amo->taskcnt;
      int64_t start = amo->start;
      int64_t end = amo->end;
      int64_t chunks = amo->chunks;
      assert(end != 0);

      auto p = ctx->m_tmap.schmach_tmap.find(ctx->m_scheduler_mid);
      assert(p != ctx->m_tmap.schmach_tmap.end());
      strads_msg(ERR, "Rank(%d) start(%ld) end(%ld) psecond->start (%ld) psecond->end(%ld) chunks(%ld) \n", 
		 ctx->rank, start, end, p->second->start, p->second->end, chunks);
      assert(p->second->start == start);
      assert(p->second->end == end);
     
      idval_pair *idvalp = (idval_pair *)calloc(end - start + 1, sizeof(idval_pair));
      assert(idvalp); // if calloc failed 

      int64_t progress=0;
      for(int64_t i=0; i < chunks; i++){
	void *msg = NULL;
	while(!msg){
	  msg = recv_ctx->pull_entry_inq(); // while loop block until I got a new message      
	}
	mbuffer *inbuf = (mbuffer *)msg;
	assert(inbuf->msg_type == SYSTEM_SCHEDULING);
	schedhead *schedhp = (schedhead *)inbuf->data;
	assert(schedhp->type == SCHED_INITVAL);
	assert(schedhp->sched_mid == ctx->m_scheduler_mid);
	int64_t entrycnt = schedhp->entrycnt;
	idval_pair *pairs = (idval_pair *)((uintptr_t)schedhp + sizeof(schedhead));

	for(int64_t j=0; j < entrycnt; j++){
	  idvalp[progress].id = pairs[j].id;
	  idvalp[progress].value = pairs[j].value;
	  progress++;
	}	

	strads_msg(INF, " [rank(%d) %ld th chunk entry(%ld) progress(%ld)] \n", 
		   ctx->rank, i, entrycnt, progress);
	recv_ctx->release_buffer((void *)msg); // don't forget this 
      }// for(int i...     

      strads_msg(ERR, "Rank(%d) chunks (%ld)  progress(%ld) taskcnt(%ld)\n", 
		 ctx->rank, chunks, progress, taskcnt);
      
      assert(progress == taskcnt); // count should be equal to progress
      for(int64_t i=0; i < taskcnt; i++){
	assert(idvalp[i].id == start + i); // ids should be contiguous from start and end 
      }

      int thrds = ctx->m_params->m_sp->m_thrds_per_scheduler;
      int basepartno = ctx->m_scheduler_mid * thrds;     
      int64_t idx=0;
      assert(ctx->m_scheduler_mid >=0);
      for(int i=0; i<thrds; i++){    
	int64_t task_start = ctx->m_tmap.schthrd_tmap[basepartno + i]->start;
	int64_t task_end = ctx->m_tmap.schthrd_tmap[basepartno + i]->end;
	//	sampling_cmd *cmd = (sampling_cmd *)calloc(1, sizeof(sampling_cmd));		
	sampling_cmd *cmd = new sampling_cmd(m_init_weight_update, task_end-task_start+1, basepartno+i);	
	assert(cmd);

	for(int k=0; k<(task_end - task_start + 1); k++){	
	  cmd->m_winfo->gidxlist[k] = idvalp[idx].id;
	  cmd->m_winfo->new_weights[k] = idvalp[idx].value;	 
	  idx++;
	}
	sthrds[i]->put_entry_inq((void*)cmd);			
      }

      assert(idx == progress);
      free(idvalp);

      mbuffer *ack = (mbuffer *)calloc(1, sizeof(mbuffer));
      ack->msg_type = SYSTEM_SCHEDULING;
      schedhead *schedhp = (schedhead *)ack->data;
      schedhp->type = SCHED_START_ACK;
      amo = (sched_start_p *)((uintptr_t)schedhp + sizeof(schedhead));
      amo->taskcnt = taskcnt;
      while(send_ctx->push_entry_outq(ack, sizeof(mbuffer)));
      recv_ctx->release_buffer((void *)mbuf); // don't forget this           
      return (void*)0x100;

    }else if(schedhp->type == SCHED_RESTART){
      sched_start_p *amo = (sched_start_p *)((uintptr_t)schedhp + sizeof(schedhead));
      int64_t taskcnt = amo->taskcnt;
      int64_t start = amo->start;
      int64_t end = amo->end;
      int64_t chunks = amo->chunks;
      assert(end != 0);

      auto p = ctx->m_tmap.schmach_tmap.find(ctx->m_scheduler_mid);
      assert(p != ctx->m_tmap.schmach_tmap.end());
      strads_msg(ERR, "Rank(%d) start(%ld) end(%ld) psecond->start (%ld) psecond->end(%ld) chunks(%ld) \n", 
		 ctx->rank, start, end, p->second->start, p->second->end, chunks);
      assert(p->second->start == start);
      assert(p->second->end == end);
      
      idval_pair *idvalp = (idval_pair *)calloc(end - start + 1, sizeof(idval_pair));
      int64_t progress=0;
      for(int64_t i=0; i < chunks; i++){

	void *msg = NULL;
	while(!msg){
	  msg = recv_ctx->pull_entry_inq(); // block until I got a new message      
	}
	mbuffer *inbuf = (mbuffer *)msg;
	assert(inbuf->msg_type == SYSTEM_SCHEDULING);
	schedhead *schedhp = (schedhead *)inbuf->data;
	assert(schedhp->type == SCHED_INITVAL);
	assert(schedhp->sched_mid == ctx->m_scheduler_mid);
	int64_t entrycnt = schedhp->entrycnt;
	idval_pair *pairs = (idval_pair *)((uintptr_t)schedhp + sizeof(schedhead));

	for(int64_t j=0; j < entrycnt; j++){
	  idvalp[progress].id = pairs[j].id;
	  idvalp[progress].value = pairs[j].value;
	  progress++;
	}	

	strads_msg(INF, " [rank(%d) %ld th chunk entry(%ld) progress(%ld)] \n", 
		   ctx->rank, i, entrycnt, progress);
	recv_ctx->release_buffer((void *)msg); // don't forget this 
      }// for(int i...     

      strads_msg(ERR, "Rank(%d) chunks (%ld)  progress(%ld) taskcnt(%ld)\n", 
		 ctx->rank, chunks, progress, taskcnt);
      
      assert(progress == taskcnt); // count should be equal to progress
      for(int64_t i=0; i < taskcnt; i++){
	assert(idvalp[i].id == start + i); // ids should be contiguous from start and end 
      }

      int thrds = ctx->m_params->m_sp->m_thrds_per_scheduler;
      int basepartno = ctx->m_scheduler_mid * thrds;     
      int64_t idx=0;
      assert(ctx->m_scheduler_mid >=0);
      for(int i=0; i<thrds; i++){    
	int64_t task_start = ctx->m_tmap.schthrd_tmap[basepartno + i]->start;
	int64_t task_end = ctx->m_tmap.schthrd_tmap[basepartno + i]->end;
	//	sampling_cmd *cmd = (sampling_cmd *)calloc(1, sizeof(sampling_cmd));		
	sampling_cmd *cmd = new sampling_cmd(m_restart_weight_update, task_end-task_start+1, basepartno+i);	
	for(int k=0; k<(task_end - task_start + 1); k++){	
	  cmd->m_winfo->gidxlist[k] = idvalp[idx].id;
	  cmd->m_winfo->new_weights[k] = idvalp[idx].value;	 
	  idx++;
	}
	sthrds[i]->put_entry_inq((void*)cmd);			
      }

      assert(idx == progress);
      free(idvalp);
      mbuffer *ack = (mbuffer *)calloc(1, sizeof(mbuffer));
      ack->msg_type = SYSTEM_SCHEDULING;
      schedhead *schedhp = (schedhead *)ack->data;
      schedhp->type = SCHED_START_ACK;
      amo = (sched_start_p *)((uintptr_t)schedhp + sizeof(schedhead));
      amo->taskcnt = taskcnt;
      while(send_ctx->push_entry_outq(ack, sizeof(mbuffer)));

    } else if(schedhp->type == SCHED_UW){ // weight update   

      schedhead *schedhp = (schedhead *)mbuf->data;
      int thrds = ctx->m_params->m_sp->m_thrds_per_scheduler;
      int basepartno = ctx->m_scheduler_mid * thrds;           
      int gthrdid = schedhp->sched_thrdgid;
      int lthrdid = gthrdid - basepartno; // only for thread context array indexing 
      int64_t entrycnt = schedhp->entrycnt;

      idval_pair *amo_idvalp = (idval_pair *)((uintptr_t)schedhp + sizeof(schedhead));
      sampling_cmd *cmd = new sampling_cmd(m_weight_update, entrycnt, gthrdid);	

      for(int64_t k=0; k < entrycnt; k++){
	cmd->m_winfo->gidxlist[k] = amo_idvalp[k].id ;
	cmd->m_winfo->new_weights[k] = amo_idvalp[k].value;	 	
      }     
      sthrds[lthrdid]->put_entry_inq((void*)cmd);			
    }else{
      LOG(FATAL) << "[scheduler] SYSTEM_SCHEDULING msg contains non-support sched-type " << endl;
    }

    recv_ctx->release_buffer((void *)mbuf); // don't forget this           

    return NULL;
  } // msg_type == SYSTEM_SCHEDULING 

  assert(0);
  return mbuf ;
}

void *scheduler_mach(void *arg){

  int rclock = 0;
  sharedctx *ctx = (sharedctx *)arg;
  int thrds = ctx->m_params->m_sp->m_thrds_per_scheduler;
  int basepartno = ctx->m_scheduler_mid * thrds;
  assert(ctx->m_scheduler_mid >=0);
  int sched_mid = ctx->m_scheduler_mid;
  strads_msg(ERR, "[scheduler-machine] rank(%d) boot up scheduler-mach (%d). create %d threads with baseline partno(%d)\n", 
	     ctx->rank, ctx->m_scheduler_mid, thrds, basepartno);

  scheduler_threadctx **sthrds = (scheduler_threadctx **)calloc(MAX_SCHEDULER_THREAD, sizeof(scheduler_threadctx *));

  for(int i=0; i<thrds; i++){    
    int64_t task_start = ctx->m_tmap.schthrd_tmap[basepartno + i]->start;
    int64_t task_end = ctx->m_tmap.schthrd_tmap[basepartno + i]->end;
    sthrds[i] = new scheduler_threadctx(ctx->rank, ctx->m_scheduler_mid, i, basepartno + i, 
					task_start, task_end, ctx->m_params->m_sp->m_bw, ctx->m_params->m_sp->m_maxset,
					ctx->m_params->m_sp->m_infthreshold, ctx);
  }

  assert(ctx->star_recvportmap.size() == 1);
  assert(ctx->star_sendportmap.size() == 1);
  auto pr = ctx->star_recvportmap.begin();
  _ringport *rport = pr->second;
  context *recv_ctx = rport->ctx;
  auto ps = ctx->star_sendportmap.begin();
  _ringport *sport = ps->second;
  context *send_ctx = sport->ctx;
  bool tflag=false;

  while (1){
    void *msg = recv_ctx->pull_entry_inq();
    if(msg != NULL){
      mbuffer *mbuf = (mbuffer *)msg;
      tflag = false;
      mbuf = (mbuffer *)process_common_system_cmd(ctx, mbuf, recv_ctx, send_ctx, &tflag);
      if(tflag == true)
	break;
      mbuf = (mbuffer *)process_sched_system_cmd(ctx, mbuf, sthrds, recv_ctx, send_ctx);
      if(mbuf != NULL)
	continue;
    }
  }


  while (1){

    void *msg = recv_ctx->pull_entry_inq();
    if(msg != NULL){
      mbuffer *mbuf = (mbuffer *)msg;
      mbuf = (mbuffer *)process_sched_system_cmd(ctx, mbuf, sthrds, recv_ctx, send_ctx);
      if(mbuf != NULL)
	continue;
    }
    while(1){
      sampling_cmd *scmd = (sampling_cmd *)sthrds[rclock]->get_entry_outq();			
      if(scmd != NULL){
	// send scmd to the coordinator 	
	mbuffer *mbuf = (mbuffer *)calloc(1, sizeof(mbuffer));
	mbuf->msg_type = SYSTEM_SCHEDULING;
	sysmsg_one<uload<task>> msg1(USER_TASK, scmd->m_winfo->size);
	for(int i=0; i< scmd->m_winfo->size; i++){
	  msg1.umember1[i].idx = scmd->m_winfo->gidxlist[i];	  
	}
	msg1.type = SCHED_PHASE;
	msg1.entrycnt = scmd->m_winfo->size;
	msg1.sched_mid = sched_mid;
	msg1.sched_thrdgid = scmd->m_samplergid;
	mbuf->src_rank = ctx->rank;
	char *tmp = msg1.serialize();
	memcpy(mbuf->data, tmp, msg1.get_blen());	
	while(send_ctx->push_entry_outq(mbuf, sizeof(mbuffer)));
	delete scmd;
	rclock++;
	rclock = rclock % thrds;
	break;
      }      
      rclock++;      
      if(rclock % thrds == 0){ 
	rclock = 0;
	break;
      }
    }
  } // end of while(1)  
  return NULL;
}

void _sort_selected(int64_t remain, int64_t *m_samples, int64_t start, int64_t end){

}

void *scheduler_thread(void *arg){ 

  scheduler_threadctx *ctx = (scheduler_threadctx *)arg; // this pointer of scheduler_threadctx class  
  int64_t selected;
  sharedctx *parenctx = (sharedctx *)ctx->m_parenctx; // parent(scheduler_mach)'s sharedctx  
  strads_msg(ERR, "[scheduler-thread] rank(%d) scheduermach(%d) schedthrd gid(%d)\n", 
	     ctx->get_rank(), ctx->get_scheduler_mid(), ctx->get_scheduler_thrdgid());		
  assert(ctx != NULL);

  wsamplerctx *wsampler = ctx->m_wsampler;
  assert(wsampler != NULL);

  DECLARE_FUNCTION(parenctx)
  pretmsg = NULL;

  while(1){
    void *recv_cmd = ctx->get_entry_inq_blocking(); 
    assert(recv_cmd != NULL);
    sampling_cmd *scmd = (sampling_cmd *)recv_cmd;
    assert(scmd != NULL);
    if(scmd->m_type == m_init_weight_update){
      assert(scmd->m_winfo->gidxlist[0] == wsampler->m_start);
      assert(scmd->m_winfo->gidxlist[scmd->m_winfo->size-1] == wsampler->m_end);
      strads_msg(ERR, "Call update init weight .... for (%ld) Rank(%d) mid(%d) thrgid(%d) \n", 
		 scmd->m_winfo->size, 
		 ctx->get_rank(), 
		 ctx->get_scheduler_mid(), 
		 ctx->get_scheduler_thrdgid());
      wsampler->update_weight(scmd->m_winfo);      
      selected = wsampler->do_sampling(wsampler->m_maxset, wsampler->m_samples);      
      assert(selected <= wsampler->m_maxset);
      string strAcol("Acol");
      dshardctx *dshard = parenctx->get_dshard_with_alias(strAcol);
      if(dshard == NULL){
	strads_msg(ERR, "no shard available for interference checking\n");
      }
#if defined(NO_INTERFERENCE_CHECK)
      int64_t remain = selected;
#else
      int64_t remain;
      if(dshard == NULL){
	strads_msg(ERR, "DSHARD IS NULL, Skip Dependency Checking\n");
	remain = selected;
      }else{
	//remain = wsampler->check_interference(wsampler->m_samples, selected, dshard);
	remain = handler->check_interference(wsampler->m_samples, selected, dshard, wsampler->m_base, wsampler->m_infthreshold);
      }
#endif
      strads_msg(INF, "maxset(%ld) Select (%ld) and (%ld) go through inference checking\n", 
		 wsampler->m_maxset, selected, remain);
      assert(remain <= wsampler->m_maxset);
      _sort_selected(remain, wsampler->m_samples, wsampler->m_start, wsampler->m_end);
      sampling_cmd *cmd = new sampling_cmd(m_make_phase, remain, wsampler->m_gid);	
      assert(cmd);
      for(int64_t k=0; k<remain; k++){	
	cmd->m_winfo->gidxlist[k] = wsampler->m_samples[k];
	cmd->m_winfo->new_weights[k] = INVALID_DOUBLE;	 
      }      
      strads_msg(ERR, "[SCHEDULER THREAD] FINISH init weight .... for (%ld) Rank(%d) mid(%d) thrgid(%d) \n", 
		 scmd->m_winfo->size, 
		 ctx->get_rank(), 
		 ctx->get_scheduler_mid(), 
		 ctx->get_scheduler_thrdgid());
    }else if(scmd->m_type == m_restart_weight_update){
      assert(scmd->m_winfo->gidxlist[0] == wsampler->m_start);
      assert(scmd->m_winfo->gidxlist[scmd->m_winfo->size-1] == wsampler->m_end);
      strads_msg(ERR, "Call update init weight .... for (%ld) Rank(%d) mid(%d) thrgid(%d) \n", 
		 scmd->m_winfo->size, 
		 ctx->get_rank(), 
		 ctx->get_scheduler_mid(), 
		 ctx->get_scheduler_thrdgid());
      wsampler->update_weight(scmd->m_winfo);      
      wsampler->m_restart_cnt ++;
      if(wsampler->m_restart_cnt == 2){
	assert(wsampler->m_restart_flag == false);
	wsampler->m_restart_flag = true;
	strads_msg(ERR, " Restart Flag for Optimization is On\n"); 
      }
      assert(wsampler->m_restart_cnt < 3);
      selected = wsampler->do_sampling(wsampler->m_maxset, wsampler->m_samples);      
      assert(selected <= wsampler->m_maxset);
      string strAcol("Acol");
      dshardctx *dshard = parenctx->get_dshard_with_alias(strAcol);
      if(dshard == NULL){
	strads_msg(ERR, "no shard available for interference checking\n");
      }
#if defined(NO_INTERFERENCE_CHECK)
      int64_t remain = selected;
#else
      int64_t remain;
      if(dshard == NULL){
	strads_msg(ERR, "DSHARD IS NULL, Skip Dependency Checking\n");
	remain = selected;
      }else{
	remain = wsampler->check_interference(wsampler->m_samples, selected, dshard);
      }
#endif
      strads_msg(INF, "maxset(%ld) Select (%ld) and (%ld) go through inference checking\n", 
		 wsampler->m_maxset, selected, remain);
      assert(remain <= wsampler->m_maxset);
      // put your m_samples into scheduler_machine 
      sampling_cmd *cmd = new sampling_cmd(m_make_phase, remain, wsampler->m_gid);	
      for(int64_t k=0; k<remain; k++){	
	cmd->m_winfo->gidxlist[k] = wsampler->m_samples[k];
	cmd->m_winfo->new_weights[k] = INVALID_DOUBLE;	 
      }      
      strads_msg(ERR, "[SCHEDULER THREAD] FINISH init weight .... for (%ld) Rank(%d) mid(%d) thrgid(%d) \n", 
		 scmd->m_winfo->size, 
		 ctx->get_rank(), 
		 ctx->get_scheduler_mid(), 
		 ctx->get_scheduler_thrdgid());
      ctx->put_entry_outq((void*)cmd);			      
    }else if(scmd->m_type == m_weight_update){
      strads_msg(INF, "Call update weight .... for (%ld) \n", scmd->m_winfo->size);
      wsampler->update_weight(scmd->m_winfo);      
      selected = wsampler->do_sampling(wsampler->m_maxset, wsampler->m_samples);      
      string strAcol("Acol");
      dshardctx *dshard = parenctx->get_dshard_with_alias(strAcol);
      if(dshard == NULL){
	strads_msg(ERR, "no shard available for interference checking\n");
      }
#if defined(NO_INTERFERENCE_CHECK)
      int64_t remain = selected;
#else
      //      assert(0);
      int64_t remain = wsampler->check_interference(wsampler->m_samples, selected, dshard);
#endif 
      if(remain < parenctx->m_params->m_sp->m_maxset*0.6)
	strads_msg(INF, "Abnormal : less than 60 percent of  maxset(%ld) Select (%ld) and (%ld) go through inference checking\n", 
		   wsampler->m_maxset, selected, remain);
      sampling_cmd *cmd = new sampling_cmd(m_make_phase, remain, wsampler->m_gid);	
      for(int64_t k=0; k<remain; k++){	
	cmd->m_winfo->gidxlist[k] = wsampler->m_samples[k];
	cmd->m_winfo->new_weights[k] = INVALID_DOUBLE;	 
      }      
      ctx->put_entry_outq((void*)cmd);			      
    }else if(scmd->m_type == m_bw_change){
      // TODO : 
    }else{
      LOG(FATAL) << "Not Supported Command detected in Scheduler Thread" << endl;
    }
    delete scmd;
  }
  return NULL;
}
