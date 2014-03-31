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
#include <boost/crc.hpp>
#include <assert.h>
#include <queue>
#include <iostream>     // std::cout
#include <algorithm>    // std::next_permutation, std::sort
#include <vector>       // std::vector
#include <ctime>        // std::time
#include <cstdlib>      // std::rand, std::srand
#include <list>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <string.h>

#include <set>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/shared_array.hpp>

#include "threadctx.hpp"
#include "basectx.hpp"
#include "utility.hpp"
#include "userdefined.hpp"
#include "strads-queue.hpp"
#include "usercom.hpp"
#include "syscom.hpp"
#include "sched-delta.hpp"

using namespace std;

static sghthreadctx **sghthctx; /* scheduler gate helper threads in scheduler machines */ 
static pthread_attr_t attr;

static void *_scheduler_gate_helper_thread(void *arg); /* scheduler machine's bg thread body */
void _get_my_column_range(threadctx *tctx, uint64_t partcnt, uint64_t features, uint64_t *startc, uint64_t *endc);
vqhandler* _init_scheduler_gate_helperthread(int sghcnt, threadctx *tctx, uint64_t startc, uint64_t endc);
//static int userdefined_init_doublebuf(threadctx *gatethctx, int lid, doublebufctx *dbufctx, int modulo);

static dpartitionctx *ooc_ready_dp;

//static tqhandler crossq;
//static wqhandler crosswq;
//static wqhandler wchannel;
static vqhandler sghchannel;
#define SETRATIO (1.5)

#define sgh_blocked 1  // 1
#define sgh_active  0  // 0


static unsigned long oocloadcnt;

static unsigned long dispatchcnt;

void *scheduler_gate_thread(void *arg){

  set<unsigned long>blockedthrds;
  set<unsigned long>activethrds;


  set<int>current_activeset;
  set<int>ready_set;

  pthread_barrier_t lbarrier;
  /* this would be shared by a gate thread and all helpers in this machines
     as far as gate threads live longer than gate helpers, this is safe. */
  threadctx *tctx = (threadctx *)arg;
  clusterctx *cctx = tctx->cctx;
  assert(tctx->schedmach != NULL);
  int mpirank = tctx->rank;
  int server_rank = tctx->cctx->totalmachines-1;
  uint8_t *recvbuf=NULL;
  int recvlen=0;
  com_header *headp=NULL;
  int firstschedm = tctx->cctx->totalmachines - tctx->cctx->schedmachines;
  int schedmrank = mpirank - firstschedm;
  test_packet *tpacket = (test_packet *)calloc(1, sizeof(test_packet));
  assert(tpacket);  
  void *msgfromhelper;
  dpartitionctx *dp;
  uint64_t startc, endc;
  int rc;

  // OOC FEATURE 
  int oocdone = 0;
  iothreadctx *ioctx = new iothreadctx;
  oocio_init_iothreadctx(ioctx, tctx);

  strads_msg(ERR, "[Sched-Machine] boots up. rank(%d) lid(%d) gid(%d) desired(%d)\n",
	     mpirank, tctx->lid, tctx->gid, tctx->schedmach->expt->desired);
  tctx->allschedbarrier = &lbarrier;
  pthread_barrier_init(tctx->allschedbarrier, NULL, cctx->schedthrd_per_mach+1);
  /* +1: thread of scheduler_gate_thread */
  // get information about my parameter space range 
  // then partition my range into multi threads
  _get_my_column_range(tctx, cctx->schedmachines-1, tctx->pctx->features, &startc, &endc);
  strads_msg(ERR, "\t\t@@@@@@@@@@@@@@@@@@@ Scheduler[%d]rank get column range (start %ld  end %ld)\n", 
	     tctx->rank, startc, endc);
  tctx->vqh = _init_scheduler_gate_helperthread(cctx->schedthrd_per_mach, tctx, startc, endc);
  strads_msg(ERR, "\t[Sched-Machine] rank(%d) invoke %d helpers  \n",
	     mpirank, cctx->schedthrd_per_mach);
  pthread_barrier_wait(tctx->allschedbarrier);
  strads_msg(ERR, "\t[Sched-Machine] rank(%d) check all %d helpers are active  \n",
	     mpirank, cctx->schedthrd_per_mach);
  pthread_barrier_wait(tctx->allschedbarrier);
  for(int i=0; i<cctx->schedthrd_per_mach; i++){
    blockedthrds.insert(i);
  }		      
  assert(activethrds.size() == 0);
  strads_msg(ERR, "\t@@@@@@@@@@@@ [Sched-Machine] rank(%d) Pass Barrier  \n", mpirank);

  while(1){
    assert(!recvbuf);
    recvlen = syscom_async_recv_amo_malloc_comhandler(tctx->comh[SGR], &recvbuf);
    if(recvlen > 0){
      assert(recvbuf);
      headp = (com_header *)recvbuf;
      // Step 2 scatter that info to appropriate local helpers 
      // TODO: move ooc checing code here 
      // OOC FEATURE Scheduler-bg-app check whether a pending job is done or not. 
      // limitation: now, it can support only one pending job at a time. 
      // check whether ioctx's pending job is done .... 
      pthread_mutex_lock(&ioctx->mu);
      if(!ioctx->retq.empty()){	
	pthread_mutex_unlock(&ioctx->mu);
	oocret_item *retitem = ioctx->retq.front();
	// //dpartitionctx->dp 
	//    tctx->awctx->dpart[0] = retitem->allocated_dpart; ///////////////////////////////////////////////////////////////
	ooc_ready_dp = retitem->allocated_dpart;
	//    delete(retitem->allocated_dpart);
	ioctx->retq.pop_front();	  
	free(retitem);
	strads_msg(ERR, "@@@@@@@@@@@@@@ SCHEDULER MachRank(%d) finish OOC IO \n", tctx->rank);
	oocdone = 1;
      }else{
	pthread_mutex_unlock(&ioctx->mu);
      }
      ////////////////////////  //////////////////////////

      if(headp->type == deltaupdate){
       	uint64_t partno = headp->partition;
	if((partno/cctx->schedthrd_per_mach) != schedmrank){
	  strads_msg(INF, "delta update message from the server has wrong partition no(%ld) \n", 
		     partno);
	  assert(false);
	} // assume sequential partitioning by column. No modulo.......
	strads_msg(INF, "GATE MACH goet DELTA UPDATE : Pass delta update(partno: %ld) function to the helper (%ld)\n", 
		   partno, partno % cctx->schedthrd_per_mach);

	if(headp->blockgh == 1){
	  strads_msg(INF, "\t@@@@@@@ partno(%ld) scheduler got deltaupdate with blocking msg\n", 
		   partno);

	}      
	vqhandler_send_f2b(tctx->vqh, (void *)recvbuf, partno % cctx->schedthrd_per_mach);	       
      }

      if(headp->type == wake_up){
       	uint64_t partno = headp->partition;
	if((partno/cctx->schedthrd_per_mach) != schedmrank){
	  strads_msg(ERR, "delta update message from the server has wrong partition no(%ld) \n", 
		     partno);
	  assert(false);
	} // assume sequential partitioning by column. No modulo.......

	uint64_t blockedlid = partno % cctx->schedthrd_per_mach; // blockedlid == lid to wake up.
	if(blockedthrds.find(blockedlid) == blockedthrds.end()){
	  strads_msg(ERR, "Fatal: try to wake up non blocked part(%ld) at mach (%d) lid(%ld)", 
		     partno, tctx->rank, blockedlid); 
	  assert(0);
	}

	// This boxed code is valid only for NULL IO TEST MODE
	// In normal workflow, this code is useless. 
	int tmpstatus = sgh_active;
	while(tmpstatus == sgh_active){
	  pthread_mutex_lock(&sghthctx[blockedlid]->mu);
	  tmpstatus = sghthctx[blockedlid]->status;
	  pthread_mutex_unlock(&sghthctx[blockedlid]->mu);
	}
	/////////////////////////////////////////////////////
	strads_msg(INF, "@@@@@@@@@@@@@@ Okay: server tries to wake up blocked part(%ld) at mach (%d) lid(%ld)\n", 
		   partno, tctx->rank, blockedlid); 	     
	pthread_cond_signal(&sghthctx[blockedlid]->wakeupsignal);
      }

      //free(recvbuf); No. Don't do that. 
      if(headp->type == load_dpart){
       	//uint64_t partno = headp->partition;       
       	uint64_t partno = headp->partition;
	strads_msg(ERR, "Scheduler Mach(%d) got loading message\n", tctx->rank);
	dpartitionctx *payload = (dpartitionctx *)&recvbuf[sizeof(com_header)];       
	dp = dpart_alloc_ctx_only(payload, tctx->rank);
	test_pkt *testpkt = (test_pkt *)calloc(1, sizeof(test_pkt));

	strads_msg(OUT, "[scheduler] rank %d loading dpartition from file [%s]\n", 
		   tctx->rank, dp->fn);
#if 0 
	dpart_dataload_memalloc_for_ctx(dp, tctx->rank);
#else
	int prangecnt = tctx->pctx->prangecnt;
	param_range *prange = tctx->pctx->prange;
	param_range *selected_prange = (param_range *)calloc(prangecnt, sizeof(param_range));
	int selected_prcnt = 0;
	int gid_to_lid;
	assert(current_activeset.size() == 0);


	strads_msg(ERR, "Scheduler Mach(%d)'s loading  message: oocdpartitions(%ld)\n", tctx->rank, dp->range.oocdpartitions);
	strads_msg(ERR, "Scheduler Mach(%d)'s loading  message: range.hmodulo(%ld)\n", tctx->rank, dp->range.h_modulo);
	

	for(int i=0; i<prangecnt; i++){

	  if( ((i % dp->range.oocdpartitions) == dp->range.h_modulo) && 
	       (prange[i].feature_start >= startc)  &&	     
	       (prange[i].feature_end <= endc)
	     ){
	    strads_msg(ERR, "SCHEDULER[%d] partno[%d] start-col(%ld)  -- end-col(%ld) gid(%d)******** \n", 
		       tctx->rank, i, prange[i].feature_start, prange[i].feature_end, prange[i].gid); 		
	    selected_prange[selected_prcnt].feature_start = prange[i].feature_start;
	    selected_prange[selected_prcnt].feature_end = prange[i].feature_end;
	    selected_prange[selected_prcnt].gid = prange[i].gid;
	    selected_prcnt++;

	    gid_to_lid = i % cctx->schedthrd_per_mach;
	    current_activeset.insert(gid_to_lid);
	  }else{
	    strads_msg(INF, "SCHEDULER[%d] partno[%d] start-col(%ld)  -- end-col(%ld) gid(%d)\n", 
		       tctx->rank, i, prange[i].feature_start, prange[i].feature_end, prange[i].gid); 		
	  }

	}      

	if(tctx->rank == 0){
	  for(int i=0; i<selected_prcnt; i++){
	    strads_msg(INF, "SCHEDULER [%d] selecte_prange[%d] startcol(%ld) endcol(%ld)\n", 
		       tctx->rank,
		       selected_prange[i].gid, 
		       selected_prange[i].feature_start, 
		       selected_prange[i].feature_end);
	  }
	}	
	dpart_dataload_memalloc_for_ctx_wt_ooc(dp, tctx->rank, selected_prange, selected_prcnt);    

#endif

	if(dp->dparttype == colmajor_matrix){
	  strads_msg(ERR, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ %d rank: col major dpart upload %ld nonzeros \n", 
		     tctx->rank, dp->spcol_mat.allocatedentry());
	  testpkt->a = dp->spcol_mat.allocatedentry();
	}else{
	  strads_msg(ERR, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ %d rank: row major dpart upload %ld nonzeros \n", 
		     tctx->rank, dp->sprow_mat.allocatedentry());
	  testpkt->a = dp->sprow_mat.allocatedentry();
	}       
	syscom_send_amo_comhandler(tctx->comh[SGR], tctx, (uint8_t *)testpkt, sizeof(test_pkt), 
				     c2s_result, server_rank, partno);  	  	  	       	
	assert(sghthctx);// at this point, sghthctx should be allocated. 	
	for(int i=0; i < cctx->schedthrd_per_mach; i++){
	  assert(sghthctx[i]);// at this point, sghthctx should be allocated. 
	  sghthctx[i]->xdp = dp;
	  sghthctx[i]->xdpready = true;
	  strads_msg(INF, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ SGH CTX XDP (%p) \n", sghthctx[i]->xdp);
	}
      }

      if(headp->type == oocload_dpart){
       	//uint64_t partno = headp->partition;       
       	uint64_t partno = headp->partition;
	strads_msg(ERR, "Scheduler Mach(%d) got OOC DPART loading message\n", tctx->rank);

	/////////////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////	
	dpartitionctx *payload = (dpartitionctx *)&recvbuf[sizeof(com_header)];		     
	strads_msg(ERR, "############################ Machine (%d) got OOC loadcmd hmodflag(%ld) modulo(%ld) oocdparts(%ld) \n", 
	       tctx->rank, payload->range.ooc_hmodflag, payload->range.h_modulo, payload->range.oocdpartitions); 
	// allocate dm context and fill it with msg from coordinator 
	dpartitionctx *dp = dpart_alloc_ctx_only(payload, tctx->rank);   
	//    show_memusage();



	if(dp->range.oocdpartitions != 1){

	  int prangecnt = tctx->pctx->prangecnt;
	  param_range *prange = tctx->pctx->prange;
	  param_range *selected_prange = (param_range *)calloc(prangecnt, sizeof(param_range));
	  int selected_prcnt = 0;

	  set<int>tmpset;
	  int gid_to_lid;

	  for(int i=0; i<prangecnt; i++){

	    if( ((i % dp->range.oocdpartitions) == dp->range.h_modulo) && 
		(prange[i].feature_start >= startc)  &&	     
		(prange[i].feature_end <= endc)
		){
	      strads_msg(ERR, "SCHEDULER[%d] partno[%d] start-col(%ld)  -- end-col(%ld) gid(%d)******** \n", 
			 tctx->rank, i, prange[i].feature_start, prange[i].feature_end, prange[i].gid); 		
	      selected_prange[selected_prcnt].feature_start = prange[i].feature_start;
	      selected_prange[selected_prcnt].feature_end = prange[i].feature_end;
	      selected_prange[selected_prcnt].gid = prange[i].gid;
	      selected_prcnt++;


	      gid_to_lid = i % cctx->schedthrd_per_mach;
	      //	    current_activeset.insert(gid_to_lid);
	      tmpset.insert(gid_to_lid);

	    }else{
	      strads_msg(INF, "SCHEDULER[%d] partno[%d] start-col(%ld)  -- end-col(%ld) gid(%d)\n", 
			 tctx->rank, i, prange[i].feature_start, prange[i].feature_end, prange[i].gid); 		
	    }

	  }      

	  if(tctx->rank == 0){
	    for(int i=0; i<selected_prcnt; i++){
	      strads_msg(INF, "SCHEDULER [%d] selecte_prange[%d] startcol(%ld) endcol(%ld)\n", 
			 tctx->rank,
			 selected_prange[i].gid, 
			 selected_prange[i].feature_start, 
			 selected_prange[i].feature_end);
	    }
	  }	

	  // TODO: Need modification for 1 ooc partition case..........
	  // swap working pointer........ 
	  if(oocloadcnt == 0){

	    assert(ooc_ready_dp == NULL);

	    // no waiting.... 
	  }else{
	 
	    int tmpstatus;
	    int checkinglid;
	    while(1){ 
	      checkinglid = -1;
	      for(set<int>::iterator it = current_activeset.begin(); it != current_activeset.end(); it++){
		checkinglid = *it;
		pthread_mutex_lock(&sghthctx[checkinglid]->mu);
		tmpstatus = sghthctx[checkinglid]->status;
		pthread_mutex_unlock(&sghthctx[checkinglid]->mu);
		if(tmpstatus == sgh_blocked){
		  break;
		}
	      }

	      if(checkinglid != -1){
		strads_msg(ERR, "@@@@@@@@@@@@@@@@@@@ Rank(%d) Current Active (%d) is blocked...\n", 
			   tctx->rank, checkinglid);
		current_activeset.erase(checkinglid);	     
	      }
	      if(current_activeset.size() == 0){
		break;
	      }
	    } // while(1) waiting for all current ones to be blocked.

	    assert(ooc_ready_dp);


	    // all gate thread look at the same one. 
	    // delete only one time.	


	    strads_msg(ERR, "Scheduler Rank[%d] delete old dp(%p) and take 120 secs. check my status\n", 
		       tctx->rank, sghthctx[0]->xdp);

	    showVmSize(tctx->rank);

	    delete(sghthctx[0]->xdp);
	    strads_msg(ERR, "!!!!!!!!!!!!!! Scheduler Rank[%d] delete done take 120 secs. check my status\n", 
		       tctx->rank);




	    sleep(10);





	    showVmSize(tctx->rank);

	    for(int i=0; i < cctx->schedthrd_per_mach; i++){
	      assert(sghthctx[i]);// at this point, sghthctx should be allocated. 
	      sghthctx[i]->xdp = ooc_ready_dp;
	      sghthctx[i]->xdpready = true;
	      strads_msg(INF, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ NEW SGH CTX XDP (%p) \n", sghthctx[i]->xdp);
	    }

	    ooc_ready_dp = NULL;


	    for(set<int>::iterator it = ready_set.begin(); it != ready_set.end(); it++){
	      current_activeset.insert(*it);
	    }
	    ready_set.erase(ready_set.begin(), ready_set.end());	 
	  } // if(oocloadcnt ==0) else ...
	

	  for(set<int>::iterator it = tmpset.begin(); it != tmpset.end(); it++){
	    ready_set.insert(*it);
	  }
	  tmpset.erase(tmpset.begin(), tmpset.end());

	  oocloadcnt++;

	  pthread_mutex_lock(&ioctx->mu);
	  if(!ioctx->bgthreadcreated){
	    rc = pthread_attr_init(&attr);
	    checkResults("pthread_attr_init failed\n", rc);
	    rc = pthread_create(&ioctx->bgthid, &attr, oocio_thread, ioctx); 
	    checkResults("pthread_create failed\n", rc);
	    ioctx->bgthreadcreated = true;       
	  }

	  // wake up threads that might be blocked
	  if(ioctx->cmdq.empty()){
	    rc = pthread_cond_signal(&ioctx->wakeupsignal);
	  }

	  ooccmd_item *item = (ooccmd_item *)calloc(1, sizeof(ooccmd_item));
	  // pass dm context to OOC iothread 
	  item->meta_dpart = dp; // dp will be deleted in OOC side. dp was allocated through new in ..ctx_only function.
	  item->selected_prange = selected_prange;
	  item->selected_prcnt = selected_prcnt;
	  item->rank = tctx->rank;
	  oocdone = 0;   
	  ioctx->cmdq.push_back(item);
	  pthread_mutex_unlock(&ioctx->mu);    
	  strads_msg(ERR, "@@@@@ SCHEDULER Machine (%d) send ooc cmd to io thread  \n", tctx->rank); 

	}else{

	  strads_msg(ERR, "@@@@@ Sched OOC DPARTITION == 1, No OOC operation. No bg IO %d rank \n", tctx->rank); 
	}


      } // end of oocload_dpart 
      recvbuf = NULL;
    } // if(recvlen > 0){

    for(int i=0; i < cctx->schedthrd_per_mach; i++){     
      msgfromhelper = vqhandler_receive_b2f(tctx->vqh, i);
      if(msgfromhelper != NULL){
	headp = (com_header *)msgfromhelper;
	if(headp->type == indsetdispatch){
	  uint64_t partno = headp->partition;
	  uint8_t *tmpbuf = (uint8_t *)msgfromhelper;
	  int size = headp->length;
	  strads_msg(INF, "SCHEDULER GATE got msg from local i(%d). partno(%ld) Send (%ld)tasks to the APP server (%d)\n", 
		     i, partno, size/sizeof(uint64_t), server_rank);
	  // TEMPORARY
#if 0 
	  syscom_send_amo_comhandler(tctx->comh[SGR], tctx, (uint8_t *)&tmpbuf[sizeof(com_header)], size, 
				     indsetdispatch, server_rank, partno);  	  	  	
#else	 
	  syscom_send_amo_comhandler_with_oocflag(tctx->comh[SGR], tctx, (uint8_t *)&tmpbuf[sizeof(com_header)], size, 
						  indsetdispatch, server_rank, partno, oocdone);  	  	  	
	  oocdone = 0;

	  // memory usage frequeycn is different from workers. 
	  // it would be much less frequent than workers.
	  dispatchcnt++;


#if defined (SHOW_VM_PEAK)
	  if(dispatchcnt % tctx->schedmach->expt->dfrequency == 0){
	    showVmPeak(tctx->rank);
	  }
#endif

#endif 	
	  free(msgfromhelper);
	}
      }
      msgfromhelper = NULL;     
    }// for(int i=0; ...       
  } // while(1)..
  return NULL;
}

void *_scheduler_gate_helper_thread(void *arg){

  sghthreadctx *tctx = (sghthreadctx *)arg;
  st_priority *priorityds = (st_priority *)calloc(1, sizeof(st_priority));
  st_sample *sampleds = (st_sample *)calloc(1, sizeof(st_sample));
  assert(priorityds);
  assert(sampleds);
  void *msgfromgate=NULL;
  com_header *headp=NULL;

  int desired = tctx->expt->desired;

  pthread_barrier_wait(tctx->allschedbarrier); 
  /* guarantee that gate thread does not access to channel until all helper threads are invoked */
  strads_msg(ERR, "\t\t\t[sgh_helper] boot rank(%d) schedid(%d) lid(%d) gid(%d) modulo(%d) -- desired(%d)\n", 
	     tctx->rank, tctx->schedid, tctx->lid, tctx->gid, tctx->modulo, desired);
  pthread_barrier_wait(tctx->allschedbarrier); 

  st_priority samplepool;
  st_sample sampleset;
  st_prioritychange pchange;
  long poolsize = tctx->end_feature - tctx->start_feature+1;
  uint64_t maxsetsize = poolsize*SETRATIO;
  assert(maxsetsize >= (unsigned int)desired);
  long localtaskid;

  psched_init_sampling(&samplepool, tctx->end_feature-tctx->start_feature+1, &sampleset, maxsetsize, &pchange, maxsetsize);
  samplepool.partno = tctx->gid;

#if defined (INTERCEPT_COL_CARE)
  uint64_t servicecnt=0;
#endif 

  strads_msg(INF, "@@@@@ Gate Helper in Partno--gid(%d) in mach(%d) is blocked. \n", tctx->gid, tctx->rank);
  pthread_mutex_lock(&tctx->mu);
  tctx->status = sgh_blocked;  

  pthread_cond_wait(&tctx->wakeupsignal, &tctx->mu);  
  strads_msg(INF, "@@@@@ Gate Helper in Partno--gid(%d) in mach(%d) ----- WAKEUP. \n", tctx->gid, tctx->rank);

  tctx->status = sgh_active; 
  pthread_mutex_unlock(&tctx->mu);

  while(1){
    msgfromgate = vqhandler_receive_f2b(tctx->vqh, tctx->lid);

    if(msgfromgate != NULL){
      // process. mainly update delta or change margin or terminate ... 
      headp = (com_header*)msgfromgate;
      uint8_t *tmp = (uint8_t *)msgfromgate;

      if(headp->type == deltaupdate){       
	delta_entry *deltalist = (delta_entry *)&tmp[sizeof(com_header)];
	uint64_t cnt = headp->length/(sizeof(delta_entry));	

	if(cnt > 0){
	  assert(deltalist[cnt-1].idx == MIN_UNIT_CHANGE_SIGNATURE);
	  samplepool.priority_minunit = deltalist[cnt-1].delta; 
	  strads_msg(INF, "My New Min Unit: %lf\n", samplepool.priority_minunit);
	  cnt --;	  
	}

	if(cnt > 0){
	  assert(deltalist[cnt-1].idx == DESIRED_CHANGE_SIGNATURE);
	  uint64_t newdesired = (uint64_t)(deltalist[cnt-1].delta); 	  	  
	  if(newdesired != desired && newdesired != 0){
	    strads_msg(ERR, "GateHelper(%d:%d:%d) Old Desired:%d --> new %ld\n", 
		       tctx->rank, tctx->lid, tctx->gid, desired, newdesired);	    
	    desired = newdesired;
	  }
	  cnt --;	  
	}

	uint64_t partition = headp->partition;		       
	strads_msg(INF, "__ scheduler gate helper thread(%ld) partition(%ld)\n", 
		   cnt, partition);       

	assert(cnt <= maxsetsize);
	pchange.changed = cnt;

	for(uint64_t i=0; i < cnt; i++){	  	  
	  localtaskid = deltalist[i].idx - tctx->start_feature;	 
	  // delta list's index are global index. 
	  // convert them into local index..  simpley .. global index - my start index... 
	  assert(localtaskid >= 0);
	  assert(localtaskid < (tctx->end_feature - tctx->start_feature +1));
	  assert(deltalist[i].idx >= tctx->start_feature);
	  assert(deltalist[i].idx <= tctx->end_feature);		
	  //strads_msg(INF, "\t\t\t ghelper-gid(%d)(%d) List gtaskid(%ld) ltaskid(%ld) == delta[%lf]\n", 
	  //	     tctx->gid, tctx->lid, deltalist[i].idx, localtaskid, deltalist[i].delta);
	  //	  pchange.taskid[i] = deltalist[i].idx;
	  pchange.taskid[i] = localtaskid;
	  pchange.newpriority[i] = deltalist[i].delta;
	}
	psched_update_priority(&samplepool, &pchange); 		
	long result = psched_do_sampling(&sampleset, &samplepool, desired);
	strads_msg(INF, "sghelper-gid(%d)Rs(%ld)Des(%d)", tctx->gid, result, desired);

#if defined(INTERFERENCE_CHECKING)
	if(result > 1){
	  if(tctx->xdpready != true){
	    strads_msg(ERR, "########### Data is not ready. Fatal  \n");
	    assert(0);
	  }
	  if(result < 400){
	    result = psched_check_interference(&sampleset, &samplepool, desired, result, tctx);
	  }else{
	    result = psched_light_check_interference(&sampleset, &samplepool, desired, result, tctx);
	  }
	}
#endif 
	int sendlength = result * sizeof(uint64_t) + sizeof(com_header);
	uint8_t *sendbuf = (uint8_t *)calloc(1, sendlength);
	com_header *sendheadp = (com_header *)sendbuf;
	uint64_t *vlist = (uint64_t *)&sendbuf[sizeof(com_header)];

	for(int i=0; i < result; i++){
	  assert(i<sampleset.rsamplesize);
	  // IMPORTANT: conver local task id to global id. 
	  vlist[i] = tctx->start_feature + sampleset.sample[i];
	  strads_msg(INF, "\t@vlist[%d] = %ld  (start_features: %ld", 
		     i, vlist[i], tctx->start_feature);	  
	}

	sendheadp->partition = tctx->gid;
	sendheadp->length = sendlength - sizeof(com_header);
	// warning: internal handling of headp requires manipulation of comheader: -sizeof(com_header)
	sendheadp->type = indsetdispatch;

	// **********************************************************************************************
	// **********************************************************************************************
	// **********************************************************************************************
	if(headp->blockgh == 1){
	  strads_msg(ERR, "@@@@@ Gate helper in Partno--gid(%d) in mach(%d) is blocked. \n", tctx->gid, tctx->rank);

	  pthread_mutex_lock(&tctx->mu);
	  tctx->status = sgh_blocked;  // 1
	  pthread_cond_wait(&tctx->wakeupsignal, &tctx->mu);
	  tctx->status = sgh_active;  // 0
	  pthread_mutex_unlock(&tctx->mu);
	  strads_msg(ERR, "@@@@@ Gate helper in Partno--gid(%d) in mach(%d) -- WAKE UP now. \n", tctx->gid, tctx->rank);
	}
	// GOTO SLEEP until Gate Thread wake up. 
	strads_msg(INF, "vqhandler-send-b2f: lid(%d)sendbuf(%p) \n", tctx->lid, sendbuf);
	vqhandler_send_b2f(tctx->vqh, (void *)sendbuf, tctx->lid);
      }// if(headp->type == deltaupdate 

      free(msgfromgate);
    } // if(msgfromgate != NULL 

  }// while(1)... 

  return NULL;
}


/* this is scheduler-client that is in charge of communicating with 
   user scheduler threads in remote nodes. 
*/
void *scheduler_client_machine(void *arg){

  threadctx *tctx = (threadctx *)arg;
  /* check sanity of context */
  clusterctx *cctx = tctx->cctx;
  assert(tctx->clictx != NULL);
  strads_msg(ERR, "\tScheduler-Worker entity boots up rank(%d) lid(%d) gid(%d). It will create %d worker threads\n",
	     tctx->rank, tctx->lid, tctx->gid, cctx->worker_per_mach);

  int mpirank = tctx->rank;
  int server_rank = tctx->cctx->totalmachines-1;
  uint8_t *recvbuf=NULL, *sendmsg=NULL; /* byte string*/
  int suc=0, blen=0, recvlen=0, rc=0;
  com_header *headp=NULL;
  uint64_t oocdone = 0;

  //  iothreadctx *ioctx = (iothreadctx *)calloc(1, sizeof(iothreadctx));
  iothreadctx *ioctx = new iothreadctx;
  oocio_init_iothreadctx(ioctx, tctx);

  suc=suc;
  blen=blen;
  recvlen=recvlen;  
  wbgctx **childs = (wbgctx **)calloc(cctx->worker_per_mach + 10, sizeof(wbgctx *));
  uint64_t sendmsglen;

  rc = pthread_attr_init(&attr);
  checkResults("pthread_attr_init failed in scheduler client machine", rc);

  for(int i=0; i < cctx->worker_per_mach; i++){   
    childs[i] = new wbgctx;
    // create worker threads 
    init_worker_threadctx(childs[i], tctx, i); 
  }

  while(1){

    oocdone = 0;
    // receive request from the app scheduler ... 
    syscom_malloc_listento_port(tctx->comh[WGR], &recvbuf); // blocking receive.... 
    headp = (com_header *)recvbuf;
    strads_msg(INF, "[WORKER-CLIENT] (%d)rank receive msg from rank(%d). dst_rank(%d) length(%d) partno(%ld) \n", 
	       mpirank, headp->src_rank, headp->dst_rank, headp->length, headp->partition);

    // YOUR JOB: implement user_client_handler_with_memalloc function in the skeleton.cpp file 
    //           input: request from the scheduler (app coordinator)
    //           output: one message that contains aggregated results from all workers in one worker machines
    sendmsg= user_client_handler_with_memalloc(tctx, childs, cctx, &sendmsglen, recvbuf, ioctx, &oocdone);

    // if send message is NULL (no response is required...)
    if(sendmsg == NULL){
      free(recvbuf);
      continue;
    }

    // send the aggregated value to the app coordinator
    strads_msg(INF, "[WORKER-CLIENT] (%d)rank Send aggregate results to the server \n", mpirank);
    syscom_send_amo_comhandler_with_oocflag(tctx->comh[WGR], tctx, (uint8_t *)sendmsg, sendmsglen, c2s_result, server_rank, -1, oocdone);
    free(recvbuf);
    free(sendmsg);
  }
  return NULL;
}

void _get_my_column_range(threadctx *tctx, uint64_t partcnt, uint64_t features, uint64_t *startc, uint64_t *endc){  

  dpartitionctx *xdarray = (dpartitionctx *)calloc(partcnt+1, sizeof(dpartitionctx)); 
  uint64_t share = (features)/partcnt;
  uint64_t remain = features % partcnt;
  uint64_t tmpend, rprog=0, myshare=0;
  uint64_t myrank = tctx->rank;
  clusterctx *cctx = tctx->cctx;

  for(uint64_t i=0; i < partcnt; i++){

    if(i==0){
      xdarray[i].range.h_s = 0;  
    }else{
      xdarray[i].range.h_s = xdarray[i-1].range.h_end + 1;  
    }

    if(rprog++ < remain){
      myshare = share +1;
    }else{
      myshare = share;
    }

    tmpend = xdarray[i].range.h_s + myshare - 1;

    if(tmpend < features){
      xdarray[i].range.h_end = tmpend;
    }else{
      xdarray[i].range.h_end = features-1;
      if(i != partcnt-1){
	strads_msg(ERR, "____ fatal error in sample partitioning. \n");
	exit(-1);
      }      
    }

    xdarray[i].range.h_len = xdarray[i].range.h_end - xdarray[i].range.h_s + 1;
  }

  *startc = xdarray[myrank - cctx->wmachines].range.h_s;
  *endc = xdarray[myrank - cctx->wmachines].range.h_end;

  free(xdarray);
  return;
}


void sched_get_my_column_range(uint64_t myrank, clusterctx *cctx, uint64_t partcnt, uint64_t features, uint64_t *startc, uint64_t *endc){  

  dpartitionctx *xdarray = (dpartitionctx *)calloc(partcnt+1, sizeof(dpartitionctx)); 
  uint64_t share = (features)/partcnt;
  uint64_t remain = features % partcnt;
  uint64_t tmpend, rprog=0, myshare=0;
  //uint64_t myrank = tctx->rank;
  //clusterctx *cctx = tctx->cctx;

  for(uint64_t i=0; i < partcnt; i++){

    if(i==0){
      xdarray[i].range.h_s = 0;  
    }else{
      xdarray[i].range.h_s = xdarray[i-1].range.h_end + 1;  
    }

    if(rprog++ < remain){
      myshare = share +1;
    }else{
      myshare = share;
    }

    tmpend = xdarray[i].range.h_s + myshare - 1;

    if(tmpend < features){
      xdarray[i].range.h_end = tmpend;
    }else{
      xdarray[i].range.h_end = features-1;
      if(i != partcnt-1){
	strads_msg(ERR, "____ fatal error in sample partitioning. \n");
	exit(-1);
      }      
    }

    xdarray[i].range.h_len = xdarray[i].range.h_end - xdarray[i].range.h_s + 1;
  }

  *startc = xdarray[myrank - cctx->wmachines].range.h_s;
  *endc = xdarray[myrank - cctx->wmachines].range.h_end;

  free(xdarray);
  return;
}


// FOR DM SUPPORT 
int userdefined_init_dpart(threadctx *gatethctx, int lid, dpartitionctx *dpartctx){ 
  return 1;
}

void _get_thread_col_range(uint64_t sghcnt, uint64_t startc, uint64_t endc, uint64_t *thrd_startc, uint64_t *thrd_endc, uint64_t mythrank){

  uint64_t partcnt = sghcnt; 
  dpartitionctx *xdarray = (dpartitionctx *)calloc(partcnt, sizeof(dpartitionctx));
  uint64_t features = endc - startc + 1;
  uint64_t share = (features)/partcnt;
  uint64_t remain = (features)%partcnt;
  uint64_t tmpend, rprog=0, myshare=0;
  for(uint64_t i=0; i<partcnt; i++){
    if(i==0){
      xdarray[i].range.h_s = 0;
    }else{
      xdarray[i].range.h_s = xdarray[i-1].range.h_end + 1;
    }
    if(rprog++ < remain){
      myshare = share +1;
    }else{
      myshare = share;
    }
    tmpend = xdarray[i].range.h_s + myshare -1;
    if(tmpend < features){
      xdarray[i].range.h_end = tmpend;
    }else{
      xdarray[i].range.h_end = features-1;
      if(i != partcnt -1){
	strads_msg(ERR, "fatal: _get_thread_col_range assignment. \n");
	assert(0);
	exit(-1);
      }
    }
  }

  for(uint64_t i=0; i<partcnt; i++){
    xdarray[i].range.h_s = xdarray[i].range.h_s + startc; 
    xdarray[i].range.h_end = xdarray[i].range.h_end + startc;     

    strads_msg(INF, " %ld th h_s (%ld) -- h_end (%ld) .... startc(%ld) endc(%ld)\n", 
	       i, xdarray[i].range.h_s, xdarray[i].range.h_end, startc, endc);
  }

  assert(xdarray[0].range.h_s == startc);
  assert(xdarray[partcnt-1].range.h_end == endc);

  *thrd_startc = xdarray[mythrank].range.h_s;
  *thrd_endc   = xdarray[mythrank].range.h_end;
}

vqhandler* _init_scheduler_gate_helperthread(int sghcnt, threadctx *tctx, uint64_t startc, uint64_t endc){

  int firstschedm = tctx->cctx->totalmachines - tctx->cctx->schedmachines;
  int rc = pthread_attr_init(&attr);
  assert(rc == 0);
  clusterctx *cctx = tctx->cctx;
  int modulo = cctx->schedthrd_per_mach*(cctx->schedmachines - 1);
  long partno = cctx->schedthrd_per_mach*(cctx->schedmachines - 1);
  vqhandler *vqh = &sghchannel;
  vqh->num = sghcnt;

  uint64_t thrd_startc;
  uint64_t thrd_endc;

  strads_msg(ERR, "\t\t\t ++ _init_scheduler_gate_helperthread sghcnt(%d) in rank(%d) partno(%ld) machrange(%ld to %ld)\n", 
	     sghcnt, tctx->rank, partno, startc, endc);

  for(int i=0; i<sghcnt; i++){
    pthread_mutex_init(&vqh->mutex_b2f[i], NULL); 
    pthread_mutex_init(&vqh->mutex_f2b[i], NULL);
  }
  for(int i=0; i<sghcnt; i++){
    assert(vqh->queue_b2f[i].empty() == true);
    assert(vqh->queue_f2b[i].empty() == true);
  }
  sghthctx = (sghthreadctx **)calloc(sghcnt, sizeof(sghthreadctx *));

  for(int i=0; i<sghcnt; i++){
    sghthctx[i] = (sghthreadctx *)calloc(1, sizeof(sghthreadctx));
    assert(sghthctx[i] != NULL);  
    sghthctx[i]->lid = i;
    sghthctx[i]->gid = tctx->cctx->schedthrd_per_mach*(tctx->rank - firstschedm)+i;
    //    sghthctx[i]->maxfeatures_per_thread = (tctx->pctx->features/partno)+1;

    _get_thread_col_range(sghcnt, startc, endc, &thrd_startc, &thrd_endc, i);
    
    //    sghthctx[i]->start_feature = sghthctx[i]->maxfeatures_per_thread *(sghthctx[i]->gid);
    //sghthctx[i]->end_feature = sghthctx[i]->start_feature + sghthctx[i]->maxfeatures_per_thread -1;
    sghthctx[i]->start_feature = thrd_startc;
    sghthctx[i]->end_feature = thrd_endc;

    if((long)sghthctx[i]->end_feature >= tctx->pctx->features){
      sghthctx[i]->end_feature = tctx->pctx->features -1;
    }
    strads_msg(ERR, "@@@@ rank(%d) lid(%d) gid(%d): feature len(%ld) startfeatures(%ld) endfeature(%ld) \n", 
	       tctx->rank, sghthctx[i]->lid, sghthctx[i]->gid, sghthctx[i]->end_feature - sghthctx[i]->start_feature+1, 
	       sghthctx[i]->start_feature, sghthctx[i]->end_feature);
    sghthctx[i]->modulo = modulo;
    sghthctx[i]->schedid = tctx->cctx->totalmachines-firstschedm;
    assert(sghthctx[i]->schedid != tctx->schedid);
    sghthctx[i]->rank = tctx->rank;
    sghthctx[i]->cctx = tctx->cctx;
    sghthctx[i]->pctx = tctx->pctx; /** PCTX ***/    
    //sghthctx[i]->pctx = tctx->pctx;
    sghthctx[i]->vqh  = vqh;  /* pass tq handler */
    sghthctx[i]->allschedbarrier = tctx->allschedbarrier;   
    // double buffering 
    sghthctx[i]->dbufctx = (doublebufctx *)calloc(1, sizeof(doublebufctx));
    sghthctx[i]->expt = tctx->schedmach->expt;    
    // TODO: init double buffering 

    //userdefined_init_doublebuf(tctx, i, sghthctx[i]->dbufctx, modulo);
    // data partitioning 
    //sghthctx[i]->dpartctx = (dpartitionctx *)calloc(1, sizeof(dpartitionctx));
    //userdefined_init_dpart(tctx, i, sghthctx[i]->dpartctx); 

    pthread_cond_init(&sghthctx[i]->wakeupsignal, NULL);
    pthread_mutex_init(&sghthctx[i]->mu, NULL);


    rc = pthread_create(&sghthctx[i]->pthid, &attr, _scheduler_gate_helper_thread, sghthctx[i]);
    assert(rc == 0);
    vqh->pthids[i] = &sghthctx[i]->pthid;

  }
  strads_msg(INF, "\t\t\t -- _init_sched_gate_helper schedmachrank(%d)(schedlid-%d) starts %d SGH thrds wt inter-thrcom\n", 
	     tctx->rank, tctx->cctx->totalmachines - firstschedm, vqh->num);
  return vqh;
}


#if 0 
// FOR ReadOnly OOC SUPPORT 
int userdefined_init_doublebuf(threadctx *gatethctx, int lid, doublebufctx *dbufctx, int modulo){

  clusterctx *cctx = gatethctx->cctx;
  //  int firstschedm = cctx->totalmachines - cctx->schedmachines;
  int total_sched_partition = cctx->schedthrd_per_mach*(cctx->schedmachines-1);
  assert(modulo == total_sched_partition);
  // equal to the total number of scheduler gate helper threads over the cluster 

  assert(gatethctx->pctx);
  long mymaxfeatures = (gatethctx->pctx->features/total_sched_partition) +1;
  long mymaxsamples  = gatethctx->pctx->samples; 

  dbufctx->_buf1_cols = mymaxfeatures;
  dbufctx->_buf1_rows = mymaxsamples;
  dbufctx->_buf1_size = -1; /* not used yet */

  double **tmpinvmat = (double **)calloc(dbufctx->_buf1_cols, sizeof(double *));
  assert(tmpinvmat);
  for(long i=0; i<dbufctx->_buf1_rows; i++){
    tmpinvmat[i] = (double *)calloc(dbufctx->_buf1_rows, sizeof(double));
    assert(tmpinvmat[i]);
  }

  strads_msg(ERR, "@@@@@  double buffer: cols(%ld) rows(%ld)\n", mymaxfeatures, mymaxsamples);
  dbufctx->invflag = true;
  dbufctx->dflag = false;
  dbufctx->initflag = true;

  //TODO: Fill buffer 
  return 1;
}
#endif 
