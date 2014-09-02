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
#include "coordinator/coordinator.hpp"
#include <glog/logging.h>
#include "./include/utility.hpp"
#include "./include/indepds.hpp"
#include "scheduler/scheduler.hpp"
#include "./include/messageclass.hpp"
#include "./include/vuserfuncs-common.hpp"
#include <gflags/gflags.h>


using namespace std;

DECLARE_int64(samples);

int  _get_tosend_machinecnt(sharedctx *ctx, machtype mtype);
void _scheduler_start_remote(sharedctx *ctx, double *weights, uint64_t wsize, bool rflag);
void _load_dshard_remote(sharedctx *ctx, userfn_entity *fne, ppacketmgt *cmdmgt);
int  _get_tosend_machinecnt(sharedctx *ctx, machtype mtype);
int  _get_totalthreads_formachtype(sharedctx *ctx, machtype mtype);
void _ending_dshardloading(sharedctx *ctx, ppacketmgt *cmdmgt);

double get_object(sharedctx *ctx);

static void _send_to_scheduler(sharedctx *ctx, mbuffer *tmpbuf, int len, int schedmid){
  while(ctx->scheduler_sendportmap[schedmid]->ctx->push_entry_outq((void *)tmpbuf, len)); 
}

static void _broadcast_to_workers(sharedctx *ctx, mbuffer *tmpbuf, int len){
  for(int i=0; i < ctx->m_worker_machines; i++){
    while(ctx->worker_sendportmap[i]->ctx->push_entry_outq((void *)tmpbuf, len));
  }
}

static void _mcopy_broadcast_to_workers(sharedctx *ctx, mbuffer *tmpbuf, int len){

  for(int i=0; i < ctx->m_worker_machines; i++){
    mbuffer *tmp = (mbuffer *)calloc(1, sizeof(mbuffer));
    memcpy(tmp, tmpbuf, sizeof(mbuffer));
    while(ctx->worker_sendportmap[i]->ctx->push_entry_outq((void *)tmp, len));
  }
  free(tmpbuf);
}

// valid for star topology
static void _send_to_workers(sharedctx *ctx, mbuffer *tmpbuf, int len, int workermid){
  while(ctx->worker_sendportmap[workermid]->ctx->push_entry_outq((void *)tmpbuf, len)); 
}

void _send_weight_update(sharedctx *ctx, int schedmid, int gthrdid, idval_pair *idvalp, int entrycnt){

  mbuffer *mbuf = (mbuffer *)calloc(1, sizeof(mbuffer)); 
  mbuf->msg_type = SYSTEM_SCHEDULING;
  int64_t dlen = USER_MSG_SIZE - sizeof(schedhead);    
  schedhead *schedhp = (schedhead *)mbuf->data;
  schedhp->type = SCHED_UW;
  schedhp->sched_mid = schedmid;
  schedhp->sched_thrdgid = gthrdid;
  schedhp->entrycnt = entrycnt;
  assert((int64_t)(entrycnt *sizeof(idval_pair)) <= dlen);
  idval_pair *amo_ldvalp = (idval_pair *)((uintptr_t)schedhp + sizeof(schedhead));
  for(int64_t k=0; k < entrycnt; k++){
    amo_ldvalp[k].id = idvalp[k].id;
    amo_ldvalp[k].value = idvalp[k].value;
  }
  _send_to_scheduler(ctx, mbuf, sizeof(mbuffer), schedmid); // once sent through com stack, it will be realeased
}

void _load_dshard_coordinator_local(sharedctx *ctx, userfn_entity *fne, ppacketmgt *cmdmgt){
  
  const char *sp = fne->m_strfn.c_str();
  const char *type = fne->m_strtype.c_str(); // row / column major 
  const char *strmachtype = fne->m_strmachtype.c_str(); // on what machines, data will be partitione
  const char *stralias = fne->m_stralias.c_str();
  machtype mtype = fne->m_mtype;
  if(mtype != m_coordinator){
    return;
  }else{
    strads_msg(ERR, "[coordinator load dshard local] file to load: [%s] assigned to [%s]\n", sp, strmachtype);
    assert(strcmp(strmachtype, "coordinator")==0);
    assert(strcmp(sp, "gen-model")==0);
    // assume that coordinator does not load any input  data from a file 
    int64_t modelsize = ctx->m_params->m_sp->m_modelsize;
    uint64_t rows=1;
    assert(modelsize > 0);
    uint64_t cols = (uint64_t)modelsize;
    assert(strcmp(type, "d2dmat") == 0);
    class dshardctx *dshard;;
    dshard = new dshardctx(rows, cols, d2dmat, sp, stralias);
    dshard->m_dmat.resize(dshard->m_rows, dshard->m_cols);
    ctx->register_shard(dshard);
    ctx->m_params->m_sp->bind_func_param(dshard->m_alias, (void *)dshard);
  }
}

void *coordinator_mach(void *arg){
  sharedctx *ctx = (sharedctx *)arg;
  strads_msg(ERR, "[coordinator-machine] rank(%d) boot up coordinator-mach \n", ctx->rank);
  assert(ctx->m_coordinator_mid >=0);

  strads_msg(ERR, "\t\tCoordinator(%d) has recvport(%lu) sendport(%lu)\n",
	     ctx->rank, ctx->star_recvportmap.size(), ctx->star_sendportmap.size()); 

  double *weights = (double *)calloc(ctx->m_params->m_sp->m_modelsize, sizeof(double));

  _scheduler_start_remote(ctx, weights, ctx->m_params->m_sp->m_modelsize, false);     

  // 10, 10 : max number of pending system command, max number of pending user commands 
  ppacketmgt *cmdmgt = new ppacketmgt(10, 10, (*ctx));

  for(auto p : ctx->m_params->m_sp->m_fnmap){   
    userfn_entity *fne = p.second;      
    machtype mtype = fne->m_mtype;
    if(mtype != m_coordinator){
      _load_dshard_remote(ctx, fne, cmdmgt); // m_scheduler or m_worker : create ds and upload data in remote machine(s) 
    }else if(mtype == m_coordinator){
      _load_dshard_coordinator_local(ctx, fne, cmdmgt); // create ds and upload data in my local machine
    }else{
      LOG(FATAL) << " Not Supported Mach Type " << endl;
    }
  }

  _ending_dshardloading(ctx, cmdmgt);

  while(1){
    for(int i = 0 ; i < ctx->m_worker_machines; i++){
      void *buf = ctx->worker_recvportmap[i]->ctx->pull_entry_inq();       
      if(buf != NULL){	  
	mbuffer *mbuf = (mbuffer *)buf;
	long cmdid = cmdmgt->mark_oneack(mbuf);
	cmdmgt->print_syscmddoneq();
	ctx->worker_recvportmap[i]->ctx->release_buffer((void *)buf);
	cmdmgt->check_cmddone(cmdid); // check and release since mark_oneack is already checked  

      }	
    }
    for(int i = 0 ; i < ctx->m_sched_machines; i++){
      void *buf = ctx->scheduler_recvportmap[i]->ctx->pull_entry_inq();       
      if(buf != NULL){	  
	mbuffer *mbuf = (mbuffer *)buf;
	long cmdid = cmdmgt->mark_oneack(mbuf);
	cmdmgt->print_syscmddoneq();
	ctx->scheduler_recvportmap[i]->ctx->release_buffer((void *)buf);
	cmdmgt->check_cmddone(cmdid); // check and release since mark_oneack is already checked  
	// do not use assert .. here. Think about pending q mechanism. 
	// only when there is no pending mach across all cluster for a given cmd, cmd is moved from 
	// pending q to done queue. Here we sent one cmd id to two machines , 	
      }	
    }
    if((cmdmgt->get_syscmddoneq_size() == 0) && (cmdmgt->get_syscmdpendq_size() == 0)){ // if all shard commanda are done;
      break;
    }
  }

  strads_msg(ERR, "[coordinator] distributed sharding is done and got confirmation\n");
  weights = (double *)calloc(ctx->m_params->m_sp->m_modelsize, sizeof(double));
  _scheduler_start_remote(ctx, weights, ctx->m_params->m_sp->m_modelsize, true);     
  // To see early breaking.

  strads_msg(ERR, "[coordinator] Send initial weight information to all schedulers to activate them\n");
  int rclock=0; // for round robin for scheduler 
  int64_t iteration=0;
  uint64_t stime = timenow();
  int64_t switch_iter=(ctx->m_params->m_sp->m_modelsize/ctx->m_params->m_sp->m_maxset)*1.2 ;

  DECLARE_FUNCTION(ctx)
	
  while(1){ // grand while loop 

    while(1){ // inner intinifite loop 
      void *buf = ctx->scheduler_recvportmap[rclock]->ctx->pull_entry_inq();       

      //      if(buf == NULL)
      //	continue;

      if(buf != NULL){	  

	/* this buf contains SCHED_PHASE message */
	mbuffer *mbuf = (mbuffer *)buf;
	assert(mbuf->msg_type == SYSTEM_SCHEDULING);
        sysmsg_one<uload<task>> *ptaskmsg;
	ptaskmsg = (sysmsg_one<uload<task>> *)mbuf->data;
	ptaskmsg->deserialize(mbuf->data, -1);
	assert(ptaskmsg->type == SCHED_PHASE);
	strads_msg(INF, "New Phase: schemid(%d) schedthrdgid(%d) entrycnt(%d)\n", 
		   ptaskmsg->sched_mid, ptaskmsg->sched_thrdgid, ptaskmsg->entrycnt);      

	// USER ROUTINE 1 
	handler->dispatch_scheduling(*ptaskmsg, &pretmsg);
	// USER ROUTINE 2
	handler->do_msgcombiner(*pretmsg, ctx);
	// USER ROUTINE 3
	handler->do_aggregate(*pretmsg, ctx);
	// USER ROUTINE 4
	handler->update_weight(*ptaskmsg, *pretmsg);

	if(pretmsg->umember1.size_ != 0){
	  for(int i=0; i<ptaskmsg->entrycnt; i++){
	    //	    ctx->m_idvalp_buf[i].id = pretmsg->umember1[i].idx;
	    ctx->m_idvalp_buf[i].id = ptaskmsg->umember1[i].idx;

#if !defined(NO_WEIGHT_SAMPLING) 
	    if(iteration > switch_iter){
	      //	      ctx->m_idvalp_buf[i].value = pretmsg->umember1[i].diff;	
	      ctx->m_idvalp_buf[i].value = ptaskmsg->umember1[i].value;	
	    }else{
	      ctx->m_idvalp_buf[i].value = 0.0;
	    }
#else
	    ctx->m_idvalp_buf[i].value = 0.0; // no sampling effects
#endif 
	  }
	}
	_send_weight_update(ctx, ptaskmsg->sched_mid,  ptaskmsg->sched_thrdgid, ctx->m_idvalp_buf, ptaskmsg->entrycnt);       		      
	ctx->scheduler_recvportmap[rclock]->ctx->release_buffer((void *)buf);
	/******************************************************************************************
	  Do not use assert .. here. Think about pending q mechanism. 
	  only when there is no pending mach across all cluster for a given cmd, cmd is moved from 
	  pending q to done queue. Here we sent one cmd id to two machines , 	
	*******************************************************************************************/
	iteration++;

	if(iteration % ctx->m_params->m_sp->m_logfreq == 0){	  
	  double psum = get_object(ctx); // collect partial object calc result from all worker machines .. 
	  uint64_t etime = timenow();	  
	  strads_msg(OUT, "\t\t %ld iter  Elaptime(%lf) sec",
		     iteration,  (etime - stime)/1000000.0);	 	 
	  usermsg_one<uload<result>> objmsg(USER_OBJTASK, 1);
	  handler->do_msgcombiner_obj(objmsg , ctx);
	  psum = objmsg.umember1[0].sqpsum;	 
	  double objectvalue = handler->do_object_aggregate(objmsg, ctx);
          ctx->write_log_total(iteration, (etime - stime), objectvalue);
	}

	if(iteration == ctx->m_params->m_sp->m_iter){
	  strads_msg(OUT, " Congratulation ! Finishing task.\nPlease, ignore any message after this message \n");	
	  sleep(5);
	  ctx->flush_log();
	  handler->log_parameters(ctx);

	  exit(0);
	}	  	

#if 1
	if(iteration == switch_iter){
	  int toflush = 0;
	  strads_msg(ERR, "I will retry SCHEDULERS \n");
	  int schedmachs = ctx->m_params->m_sp->m_schedulers;
	  int thrds_per_sched = ctx->m_params->m_sp->m_thrds_per_scheduler;
	  int schedthrds = schedmachs * thrds_per_sched;
	  while(1){
	    for(int m=0; m < schedmachs; m++){
	      void *buf = ctx->scheduler_recvportmap[m]->ctx->pull_entry_inq();       
	      if(buf != NULL){	  
		ctx->scheduler_recvportmap[m]->ctx->release_buffer(buf);
		toflush++;
	      }
	    }
	    
	    if(toflush == schedthrds)
	      break;
	  }
	  strads_msg(ERR, "I could Restart SCHEDULERS \n");
	  weights = (double *)calloc(ctx->m_params->m_sp->m_modelsize, sizeof(double));
	  handler->set_initial_priority(weights, ctx->m_params->m_sp->m_modelsize);
	  _scheduler_start_remote(ctx, weights, ctx->m_params->m_sp->m_modelsize, true);     
	} // if(iteration == 12000 ) ... reset scheduler 
#endif 
	rclock++;
	rclock = rclock % ctx->m_sched_machines; 
	break;
      
      }// if(buf != NULL .... statement 
      rclock++;
      if(rclock % ctx->m_sched_machines == 0){
	rclock = 0;
	break;
      }      
    } // while (1) -- inner infinite loop 
  } // end of outer grand while loop
  return NULL;
}

double get_object(sharedctx *ctx){

  mbuffer *task = (mbuffer *)calloc(1, sizeof(mbuffer));
  task->msg_type = USER_PROGRESS_CHECK;
  workhead *workhp =  (workhead *)task->data;
  workhp->type = WORK_OBJECT;
  usermsg_one<uload<result>> uobjmsg(USER_OBJTASK, 1);  
  char *tmp = uobjmsg.serialize();
  memcpy(task->data, tmp, uobjmsg.get_blen());
  _mcopy_broadcast_to_workers(ctx, task, sizeof(mbuffer));
  // task will be freed in _mcopy_broadcast_to_workers routine 

  // free temporary memory alloc by serialize routine
  free(tmp);
  double psum =0;

  return psum;
}

int _get_tosend_machinecnt(sharedctx *ctx, machtype mtype){
  int ret;
  if(mtype == m_worker){
    ret = ctx->m_worker_machines;
  }else if(mtype == m_scheduler){
    ret = ctx->m_sched_machines;
  }else if(mtype == m_coordinator){
    ret = 1;
  }else{
    LOG(FATAL) << "get tosend machinecnt : not supported type " << endl;
  }
  return ret;
}

int _get_totalthreads_formachtype(sharedctx *ctx, machtype mtype){
  int ret;  
  if(mtype == m_worker){
    ret = ctx->m_worker_machines * ctx->m_params->m_sp->m_thrds_per_worker ;
  }else if(mtype == m_scheduler){
    ret = ctx->m_sched_machines * ctx->m_params->m_sp->m_thrds_per_scheduler;
  }else if(mtype == m_coordinator){
    ret = 1*ctx->m_params->m_sp->m_thrds_per_coordinator;
  }else{
    LOG(FATAL) << "get tosend machinecnt : not supported type " << endl;
  }
  return ret;
}

void _load_dshard_remote(sharedctx *ctx, userfn_entity *fne, ppacketmgt *cmdmgt){
  const char *sp = fne->m_strfn.c_str();
  const char *type = fne->m_strtype.c_str(); // row / column major 
  const char *pscheme = fne->m_strpscheme.c_str(); // partitioning scheme..
  const char *strmachtype = fne->m_strmachtype.c_str(); // on what machines, data will be partitione
  const char *stralias = fne->m_stralias.c_str();
  machtype mtype = fne->m_mtype;
  strads_msg(ERR, "[coordinator load dshard remote] file to load: [%s] assigned to [%s]\n", sp, strmachtype);
  uint64_t rows, cols, nz;


  //  if(strcmp(sp, "empty")!= 0){
  if(strcmp(sp, IN_MEMORY_DS_STRING)!= 0){
    iohandler_spmat_pbfio_read_size(sp, &rows, &cols, &nz); 
  }else{
    // if gen model 
    //    rows = ctx->m_params->m_sp->m_samples;  // this is not always true
    rows = FLAGS_samples;  // this is not always true 
    // TODO : add more options for the gen model data structure 
    // in the case of SVM, it is subject to change. 
    cols = 1;
    strads_msg(OUT, "WARNING: inmemory type data structure is initialized with row = samples, col = 1\n");
    //    assert(0);
    // TODO : later, allow users to set their in memory data size 
  }

  strads_msg(ERR, "\t\tdata summary: rows(%ld) cols(%ld) nz(%ld)\n", rows, cols, nz);
  // row major format since row major partitioning  
  class dshardctx *dshard;;
  shard_info *shardmaps;

  if( (strcmp(type, "rmspt")== 0) || (strcmp(type, "rvspt")== 0) ){
    //    dshard = new dshardctx(rows, cols, rmspt, sp);
    if(strcmp(type, "rmspt")==0){
      dshard = new dshardctx(rows, cols, rmspt, sp, stralias);
    }else if(strcmp(type, "rvspt")==0){
      dshard = new dshardctx(rows, cols, rvspt, sp, stralias);
    }

    shardmaps = new shard_info;
    int machines = _get_tosend_machinecnt(ctx, mtype); // currently, data assignment support scheduler / worker grain . 
    map<int, range *>tmap;
    int finepartitions = _get_totalthreads_formachtype(ctx, mtype);
    if(strcmp(pscheme, "row") == 0){
      dshard_make_finepartition(rows, finepartitions, tmap, ctx->rank, true);
    }else if(strcmp(pscheme, "col") == 0){
      dshard_make_finepartition(cols, finepartitions, tmap, ctx->rank, true);      
    }else{
      assert(0);
    }
    dshard_make_superpartition(machines, tmap, shardmaps->finemap, ctx->rank, true);
  }else if( (strcmp(type, "cmspt")==0) || (strcmp(type, "cvspt")==0)){

    if(strcmp(type, "cmspt")==0){
      dshard = new dshardctx(rows, cols, cmspt, sp, stralias);
    }else{
      dshard = new dshardctx(rows, cols, cvspt, sp, stralias);
    }     
    shardmaps = new shard_info;
    int machines = _get_tosend_machinecnt(ctx, mtype);

    map<int, range *>tmap;
    int finepartitions = _get_totalthreads_formachtype(ctx, mtype);

    if(strcmp(pscheme, "row") == 0){
      dshard_make_finepartition(rows, finepartitions, tmap, ctx->rank, true);
    }else if(strcmp(pscheme, "col") == 0){
      dshard_make_finepartition(cols, finepartitions, tmap, ctx->rank, true);      
    }else{
      assert(0);
    }

    dshard_make_superpartition(machines, tmap, shardmaps->finemap, ctx->rank, true);

  }else if (strcmp(type, "d2dmat") == 0) {
    dshard = new dshardctx(rows, cols, d2dmat, sp, stralias);
    shardmaps = new shard_info;
    int machines = _get_tosend_machinecnt(ctx, mtype);

    map<int, range *>tmap;
    int finepartitions = _get_totalthreads_formachtype(ctx, mtype);

    if(strcmp(pscheme, "row") == 0){
      dshard_make_finepartition(rows, finepartitions, tmap, ctx->rank, true);
    }else if(strcmp(pscheme, "col") == 0){
      dshard_make_finepartition(cols, finepartitions, tmap, ctx->rank, true);      
    }else if(strcmp(pscheme, "NO") == 0){

    }else{
      assert(0);
    }

    dshard_make_superpartition(machines, tmap, shardmaps->finemap, ctx->rank, true);

  }else{
    LOG(FATAL) << "user data type that is not supported " << type << endl;
  }

  long cmdid = cmdmgt->get_cmdclock();
  cmdmgt->push_cmdq(cmdid, mtype);
  int machines = _get_tosend_machinecnt(ctx, mtype);
  for(int k=0; k < machines; k++){
    rangectx *rctx = new rangectx;
    if(strcmp(pscheme, "row")== 0){
      rctx->r_start = shardmaps->finemap[k]->start;
      rctx->r_end = shardmaps->finemap[k]->end;
      rctx->r_len = rctx->r_end - rctx->r_start + 1;
      rctx->c_start = 0; // since row partition, each machine cover all columns
      rctx->c_end = cols-1;
      rctx->c_len = rctx->c_end - rctx->c_start+1;
    } else if( strcmp(pscheme, "col")==  0){
      rctx->r_start = 0; // since column partition, each machine cover all rows 
      rctx->r_end = rows-1;
      rctx->r_len = rctx->r_end - rctx->r_start + 1;
      rctx->c_start = shardmaps->finemap[k]->start;
      rctx->c_end = shardmaps->finemap[k]->end;
      rctx->c_len = rctx->c_end - rctx->c_start + 1;
      strads_msg(ERR, "@@@@@ i[%d] c_start(%ld) c_end(%ld)\n", 
		 k, rctx->c_start, rctx->c_end);
    }else{
      assert(0); // not yet supported format.
    }

    dshard->set_range(rctx); // Don't forget this 

    mini_dshardctx *mshard = new mini_dshardctx(dshard->m_rows, dshard->m_cols, dshard->m_mpartid, 
						dshard->m_type, dshard->m_fn, &dshard->m_range, 
						dshard->m_finecnt, dshard->m_supercnt, dshard->m_alias); 

    mbuffer *mbuf = (mbuffer *)calloc(1, sizeof(mbuffer));
    sys_packet *spacket = (sys_packet *)mbuf->data;
    mbuf->msg_type = SYSTEM_DSHARD;
    memcpy((void *)spacket->syscmd, (void *)mshard, sizeof(mini_dshardctx));

    mbuf->cmdid = cmdid;

    if(mtype == m_worker){
      _send_to_workers(ctx, mbuf, sizeof(mbuffer), k);
    }else if(mtype == m_scheduler){
      _send_to_scheduler(ctx, mbuf, sizeof(mbuffer), k);
    }else{
      LOG(FATAL) << " load_dshard_remote : not yet supported machien type" << endl;
    }

  } // for(int k = 0 ; k < machines 






}

void _ending_dshardloading(sharedctx *ctx, ppacketmgt *cmdmgt){
  long cmdid = cmdmgt->get_cmdclock();
  cmdmgt->push_cmdq(cmdid, m_worker);

  int machines = _get_tosend_machinecnt(ctx, m_worker);
  for(int k=0; k < machines; k++){
    mbuffer *mbuf = (mbuffer *)calloc(1, sizeof(mbuffer));
    mbuf->msg_type = SYSTEM_DSHARD_END;
    mbuf->cmdid = cmdid;
    _send_to_workers(ctx, mbuf, sizeof(mbuffer), k);
  }  
  cmdid = cmdmgt->get_cmdclock();
  cmdmgt->push_cmdq(cmdid, m_scheduler);
  machines = _get_tosend_machinecnt(ctx, m_scheduler);
  for(int k=0; k < machines; k++){  
    mbuffer *mbuf = (mbuffer *)calloc(1, sizeof(mbuffer));
    mbuf->msg_type = SYSTEM_DSHARD_END;
    mbuf->cmdid = cmdid;
    _send_to_scheduler(ctx, mbuf, sizeof(mbuffer), k);
  }
}

void _scheduler_start_remote(sharedctx *ctx, double *weights, uint64_t wsize, bool rflag){
  int machines = _get_tosend_machinecnt(ctx, m_scheduler);
  strads_msg(ERR, "Machines : Scheduler %d \n", machines);
  for(int i = 0; i < machines; i++){
    mbuffer *mbuf = (mbuffer *)calloc(1, sizeof(mbuffer));
    mbuf->msg_type = SYSTEM_SCHEDULING;
    mbuf->src_rank = ctx->rank;
    schedhead *schedhp = (schedhead *)mbuf->data;

    int64_t dlen = USER_MSG_SIZE - sizeof(schedhead);    

    if(rflag == true){
      schedhp->type = SCHED_RESTART;
    }else{
      schedhp->type = SCHED_START;
    }

    schedhp->sched_mid = i;
    schedhp->sched_thrdgid = -1; // not for a specific threads, it's for a scheduler machine
    schedhp->entrycnt = 1;
    schedhp->dlen = sizeof(sched_start_p);

    sched_start_p *amo = (sched_start_p *)((uintptr_t)schedhp + sizeof(schedhead));
    auto p = ctx->m_tmap.schmach_tmap.find(i);
    assert(p != ctx->m_tmap.schmach_tmap.end());

    int64_t taskcnt = p->second->end - p->second->start +1;
    int64_t start = p->second->start; 
    int64_t end = p->second->end; 
    int64_t entry_per_chunk = dlen / (2*sizeof(idval_pair));
    int64_t chunks = taskcnt / entry_per_chunk;
   
    if( taskcnt % entry_per_chunk == 0){
      // no remain in amo->chunks = (taskcnt)*(2*sizeof(idval_pair)) / dlen
    }else{
      chunks++; // since there was remain in the  amo->chunks = (taskcnt)*(2*sizeof(idval_pair)) / dlen;
    }

    amo->taskcnt = taskcnt;
    amo->start = start;
    amo->end = end;
    amo->chunks = chunks;    
    strads_msg(ERR, "[Coordinator start remote] for %d schedmach start(%ld) end(%ld) chunks(%ld)\n", 
	       i, start, end, chunks);

    _send_to_scheduler(ctx, mbuf, sizeof(mbuffer), i); // once sent through com stack, it will be realeased

    int64_t progress=0;
    assert(entry_per_chunk * chunks >= taskcnt);

#if !defined(COM_RDMA_PRIMITIVES)
    if(chunks > MAX_ZMQ_HWM - 10){
      strads_msg(ERR, "ZMQ HIGH MARK WAS SET TO TOO LOW.\n");
      assert(0);
    }
#endif 

    for(int64_t ci=0; ci < chunks; ci++){      

      mbuf = (mbuffer *)calloc(1, sizeof(mbuffer));
      mbuf->msg_type = SYSTEM_SCHEDULING;
      mbuf->src_rank = ctx->rank;
      schedhp = (schedhead *)mbuf->data;
      idval_pair *pairs = (idval_pair *)((uintptr_t)schedhp + sizeof(schedhead));
      int64_t entrycnt = 0;
      for(int64_t ei=0; ei < entry_per_chunk; ei++){       
	pairs[ei].value = weights[start + ci*entry_per_chunk + ei];
	pairs[ei].id = start + ci*entry_per_chunk + ei;
	progress++;
	entrycnt++;
	if(progress == taskcnt){
	  break;
	}
      }
      schedhp->type = SCHED_INITVAL;
      schedhp->sched_mid = i;
      schedhp->sched_thrdgid = -1;
      schedhp->entrycnt = entrycnt;
      schedhp->dlen = entrycnt*sizeof(idval_pair); 
      _send_to_scheduler(ctx, mbuf, sizeof(mbuffer), i); // once sent through com stack, it will be realeased
      strads_msg(INF, "[Coordinator start remote] for %d schedmach start(%ld) end(%ld) chunks(%ld) process %ld chunks\n", 
      		 i, start, end, chunks, ci);    
      if(ci % 900 == 0){
	//	sleep(1); // for prevent overflow ...... 
      }
    }

    void *buf;
    while(1){
      buf = ctx->scheduler_recvportmap[i]->ctx->pull_entry_inq();       
      if(buf != NULL){
	break;
      }
    }

    if(buf != NULL){	  
      mbuffer *mbuf = (mbuffer *)buf;
      assert(mbuf->msg_type == SYSTEM_SCHEDULING);
      schedhead *schedhp = (schedhead *)mbuf->data;

      assert(schedhp->type == SCHED_START_ACK);

      sched_start_p *amo = (sched_start_p *)((uintptr_t)schedhp + sizeof(schedhead));      
      int64_t rtaskcnt = amo->taskcnt;
      if(rtaskcnt != taskcnt){ // sent taskcnt in coordinator and received taskcnt in remote side not match
	LOG(FATAL) << "Mismatch of taskcnt in remote side Expected: "<< taskcnt << " received: " << rtaskcnt << endl;
      }else{
	strads_msg(ERR, "\t\t init weight receive ACK from sched_mid(%d)\n", i);
      }
      ctx->scheduler_recvportmap[i]->ctx->release_buffer((void *)buf);
    }	
    strads_msg(ERR, "\tinit weigt to sched_mid(%d) -- done \n", i);
  }  
}
