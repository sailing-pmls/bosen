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
/* Memory Alloc Policy 
   Don't do dynamic allocation and deallocation. 
   In the case of desired set 20 in dense algorithm, 
   use of dynamic memory allocation slow down speed by three times. 

   Policy: allocate a pool of packets, and reuse them repeatedly without
           deallocation. */
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

#include "threadctx.hpp"
#include "basectx.hpp"
#include "utility.hpp"
#include "userdefined.hpp"
#include "strads-queue.hpp"
//#include "corrgraph.hpp"
#include "usercom.hpp"
#include "sched-delta.hpp"
#include "dpartitioner.hpp"
#include "syscom.hpp"
#include "interthreadcom.hpp"

using namespace std;

static pthread_attr_t attr;

uint8_t scratch[1024*1024];
// todo replace 1024 * 1024 with BETA_MAX ... 
idmpartial_pair idmp[1024*1024];

uint64_t oocloading;
static dpartitionctx *ooc_ready_dp;
void _make_thread_dplan_by_sample(threadctx *tctx, dpartitionctx *mdp){

  strads_msg(ERR, "__make thread data plan by sample \n\n");

  uint64_t threads = tctx->cctx->worker_per_mach;
  uint64_t mrow_start, mrow_end, mrow_len, tmpend;

  appworker *ap = tctx->awctx;
  rangectx *thrange = ap->thrange;

  mrow_start = mdp->range.v_s;
  mrow_end = mdp->range.v_end;
  mrow_len = mdp->range.v_len;
  
  assert((mrow_end-mrow_start+1) == mrow_len);
  assert(mrow_len != 0);

  //  uint64_t share = mrow_len / threads +1;
  uint64_t share = mrow_len / threads;
  uint64_t remain = mrow_len % threads;
  uint64_t myshare;
  uint64_t rprog=0;

  strads_msg(ERR, " %d rank remain %ld share: %ld\n", 
	     tctx->rank, remain, share);

  for(uint64_t i=0; i<threads; i++){    

    if(i==0){
      thrange[i].v_s = mrow_start + 0;
    }else{
      thrange[i].v_s = thrange[i-1].v_end + 1;
    }

    if(rprog++ < remain){
      myshare = share+1;
    }else{
      myshare = share;
    }
    
    tmpend = thrange[i].v_s + myshare - 1;

    strads_msg(INF, "myshare: %ld vs [%ld]\n", 
	       myshare, thrange[i].v_s);
    
    if(tmpend <= mrow_end){
      thrange[i].v_end = tmpend;
    }else{
      thrange[i].v_end = mrow_end;
      if(i != threads-1){
	strads_msg(ERR, "@@@@@@@@ ______ fatal _make_thread_dplan_by_sample \n");
	assert(0);
	exit(-1);
      }
    }
    thrange[i].v_len = thrange[i].v_end - thrange[i].v_s + 1;
    strads_msg(OUT, "\t[worker] (mid-%d:lid-%ld) assigned to samples from (%ld) to (%ld), samnplecnt(%ld)\n", 
	       tctx->rank, i, thrange[i].v_s, thrange[i].v_end, thrange[i].v_len);
  }

}

// user's scheduler client side aggregator/scatter 
// TODO: MOVE TO APP DIRECTORY's user-scheduler.cpp file. 
uint8_t *user_client_handler_with_memalloc(threadctx *tctx, wbgctx **childs, clusterctx *cctx, uint64_t *sendmsglen, uint8_t *recvbuf, iothreadctx *ioctx, uint64_t *oocdone){

  int rc;
  com_header *headp = (com_header *)recvbuf;
  uint8_t *retpacket = NULL;
  c2s_meta c2smeta;
  uint64_t pos;
  double *beta = tctx->awctx->beta;;

  if(headp->type == load_dpart){
    strads_msg(ERR, "Machine (%d) got loading message \n", tctx->rank); 
    dpartitionctx *payload = (dpartitionctx *)&recvbuf[sizeof(com_header)];		     
    dpartitionctx *dp = dpart_alloc_ctx_only(payload, tctx->rank);   
    // TODO : REPLACE with with data load confirm message......... 
    test_pkt *testpkt = (test_pkt *)calloc(1, sizeof(test_pkt));
    if(tctx->awctx->dpartslot == 0){

      strads_msg(ERR, "############### IN clientmach: dp->range.ooc(flag:%ld, modulo:%ld, oocparts:%ld\n", 
		 dp->range.ooc_hmodflag, dp->range.h_modulo, dp->range.oocdpartitions);
      //      show_memusage();
#if 0 
      dpart_dataload_memalloc_for_ctx(dp, tctx->rank);
#else
      int prangecnt = tctx->pctx->prangecnt;
      param_range *prange = tctx->pctx->prange;
      param_range *selected_prange = (param_range *)calloc(prangecnt, sizeof(param_range));

      int selected_prcnt = 0;

      for(int i=0; i<prangecnt; i++){
	if((i % dp->range.oocdpartitions) == dp->range.h_modulo){
	  strads_msg(INF, "partno[%d] start-col(%ld)  -- end-col(%ld) gid(%d)******** \n", 
		     i, prange[i].feature_start, prange[i].feature_end, prange[i].gid); 		
	  selected_prange[selected_prcnt].feature_start = prange[i].feature_start;
	  selected_prange[selected_prcnt].feature_end = prange[i].feature_end;
	  selected_prange[selected_prcnt].gid = prange[i].gid;
	  selected_prcnt++;
	}else{
	  strads_msg(INF, "partno[%d] start-col(%ld)  -- end-col(%ld) gid(%d)\n", 
		     i, prange[i].feature_start, prange[i].feature_end, prange[i].gid); 		
	}
      }      

      if(tctx->rank == 0){
	for(int i=0; i<prangecnt; i++){
	  if((i % dp->range.oocdpartitions) == dp->range.h_modulo){
	    strads_msg(ERR, "@@@@@ Worker partno[%d] start-col(%ld)  -- end-col(%ld) gid(%d)******** \n", 
		       i, prange[i].feature_start, prange[i].feature_end, prange[i].gid); 		
	  }else{
	    strads_msg(INF, "@@@@@ Worker partno[%d] start-col(%ld)  -- end-col(%ld) gid(%d)\n", 
		       i, prange[i].feature_start, prange[i].feature_end, prange[i].gid); 		
	  }
	}      

	for(int i=0; i<selected_prcnt; i++){
	  strads_msg(INF, "@@@@@ selecte_prange[%d] startcol(%ld) endcol(%ld)\n", 
		     selected_prange[i].gid, 
		     selected_prange[i].feature_start, 
		     selected_prange[i].feature_end);
	}
      }
      dpart_dataload_memalloc_for_ctx_wt_ooc(dp, tctx->rank, selected_prange, selected_prcnt);
#endif

      //      show_memusage();

      if(dp->dparttype == colmajor_matrix){
	strads_msg(INF, "@@@@@ %d rank: colmajor dpart upload %ld nonzeros \n", 
		   tctx->rank, dp->spcol_mat.allocatedentry());
	testpkt->a = dp->spcol_mat.allocatedentry();

      }else{
	strads_msg(INF, "@@@@@ %d rank: rowmajor dpart upload %ld nonzeros \n", 
		   tctx->rank, dp->sprow_mat.allocatedentry());
	testpkt->a = dp->sprow_mat.allocatedentry();
      }   

      *sendmsglen = sizeof(test_pkt);
      //    return (uint8_t *)testpkt;
      retpacket = (uint8_t *)testpkt;
      if(tctx->awctx->dpartslot == 0){
	init_client_thread(tctx, dp->fullsamples, dp->fullfeatures);
	_make_thread_dplan_by_sample(tctx, dp);
      }
      tctx->awctx->dpart[tctx->awctx->dpartslot++] = dp;

      for(uint64_t i=0; i < tctx->awctx->dpartslot; i++){
	strads_msg(OUT, "[worker] rank %d loading %ld dpartition from file [%s], dp(%p)\n", 
		   tctx->rank, i, tctx->awctx->dpart[i]->fn, dp);
      }
    }else{
      assert(tctx->awctx->dpartslot == 1);
      // allow only two data loading for big lasso.
      strads_msg(OUT, "[worker] rank %d Read Y vector(%ld) from %s file Resp(%p)\n", 
		 tctx->rank, dp->fullsamples, dp->fn, tctx->awctx->residual); 
      lasso_read_y((uint64_t)tctx->rank, dp->fn, dp->fullsamples, tctx->awctx->residual);
      for(uint64_t i=0; i<dp->fullsamples; i++){
	tctx->awctx->yvector[i] = tctx->awctx->residual[i];	
      }

      testpkt->a = dp->fullsamples;
      *sendmsglen = sizeof(test_pkt);
      retpacket = (uint8_t *)testpkt;      
    }

  } // headp->type == load_dpart ...

#if 1
  if(headp->type == oocload_dpart){

    strads_msg(INF, "@@@ Machine (%d) got OOC loading message \n", tctx->rank); 
    dpartitionctx *payload = (dpartitionctx *)&recvbuf[sizeof(com_header)];		     
    strads_msg(ERR, "@@@ Worker Machine (%d) got OOC loadcmd hmodflag(%ld) modulo(%ld) oocdparts(%ld) \n", 
	       tctx->rank, payload->range.ooc_hmodflag, payload->range.h_modulo, payload->range.oocdpartitions); 
    // allocate dm context and fill it with msg from coordinator 
    dpartitionctx *dp = dpart_alloc_ctx_only(payload, tctx->rank);   
    //    show_memusage();
    int prangecnt = tctx->pctx->prangecnt;
    param_range *prange = tctx->pctx->prange;
    param_range *selected_prange = (param_range *)calloc(prangecnt, sizeof(param_range));

    int selected_prcnt = 0;


    if(dp->range.oocdpartitions != 1){

      for(int i=0; i<prangecnt; i++){
	if((i % dp->range.oocdpartitions) == dp->range.h_modulo){
	  strads_msg(INF, "partno[%d] start-col(%ld)  -- end-col(%ld) gid(%d)******** \n", 
		     i, prange[i].feature_start, prange[i].feature_end, prange[i].gid); 		
	  selected_prange[selected_prcnt].feature_start = prange[i].feature_start;
	  selected_prange[selected_prcnt].feature_end = prange[i].feature_end;
	  selected_prange[selected_prcnt].gid = prange[i].gid;
	  selected_prcnt++;
	}else{
	  strads_msg(INF, "partno[%d] start-col(%ld)  -- end-col(%ld) gid(%d)\n", 
		     i, prange[i].feature_start, prange[i].feature_end, prange[i].gid); 		
	}
      }      

      if(tctx->rank == 0){
	for(int i=0; i<prangecnt; i++){
	  if((i % dp->range.oocdpartitions) == dp->range.h_modulo){
	    strads_msg(ERR, "partno[%d] start-col(%ld)  -- end-col(%ld) gid(%d)******** \n", 
		       i, prange[i].feature_start, prange[i].feature_end, prange[i].gid); 		
	  }else{
	    strads_msg(INF, "partno[%d] start-col(%ld)  -- end-col(%ld) gid(%d)\n", 
		       i, prange[i].feature_start, prange[i].feature_end, prange[i].gid); 		
	  }
	}      

	for(int i=0; i<selected_prcnt; i++){
	  strads_msg(INF, "selecte_prange[%d] startcol(%ld) endcol(%ld)\n", 
		     selected_prange[i].gid, 
		     selected_prange[i].feature_start, 
		     selected_prange[i].feature_end);
	}
      }
      // TODO:
      // change loading function //////////////////////////////////////////////////////////////////////////////////////////
    
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

      ioctx->cmdq.push_back(item);
      pthread_mutex_unlock(&ioctx->mu);
    }else{
      strads_msg(ERR, "DPARTITION == 1: Never do ooc and hidden IO operation\n");
    }
    
    test_pkt *testpkt = (test_pkt *)calloc(1, sizeof(test_pkt));
    testpkt->a = 0;
    *sendmsglen = sizeof(test_pkt);
    retpacket = (uint8_t *)testpkt;      
    retpacket = NULL;
    *sendmsglen = 0;



    if(ooc_ready_dp != NULL){

#if defined(OOC_TEST_NULL_IO)
      strads_msg(ERR, "OOC TEST NULL IO  should be turned off .... ");    
      assert(0);
      exit(0);
#endif
      
      strads_msg(ERR, "!!!!!!!!!!!!!!!!!! WORKER[%d] to new dp[%p]   delete olddp(%p) @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n", 
		 tctx->rank, ooc_ready_dp, tctx->awctx->dpart[0]);     
      showVmSize(tctx->rank);

      delete(tctx->awctx->dpart[0]);

      strads_msg(ERR, "!!!!!!!!!!!!!!!!!! Worker My Rank [%d] I free Old dp. and wait for 120 sec. Check my status@@@\n", 
		 tctx->rank);     

      sleep(10);

      showVmSize(tctx->rank);

      // TODO: Make sure thant after this point, no one refers to old dp.... any more \n")
      tctx->awctx->dpart[0] = ooc_ready_dp; ///////////////////////////////////////////////////////////////      


      ooc_ready_dp = NULL;


    }
    strads_msg(INF, "@@@@@ Machine (%d) send ooc cmd to io thread  \n", tctx->rank); 
  } // headp->type == load_dpart ...
#endif 

  if(headp->type == s2c_update){


    //strads_msg(ERR, "@@@@@@@@@@@@@@@@@  START UPDATE \n", tctx->rank);
    //    show_memusage();


    pos = sizeof(com_header);
    s2c_meta *ps2cmeta = (s2c_meta *)&recvbuf[pos];
    pos += sizeof(s2c_meta);

    uint64_t nbc = ps2cmeta->nbc;
    uint64_t upc = ps2cmeta->upc;

    idval_pair *betaidval = (idval_pair *)&recvbuf[pos];
    pos += nbc*sizeof(idval_pair);

    uint64_t *pvidlist = (uint64_t *)&recvbuf[pos];
    pos += upc*sizeof(uint64_t);

    end_meta *pendmeta = (end_meta *)&recvbuf[pos];
    assert(pendmeta->end_signature == ENDSIGNATURE);

    for(int i=0; i < cctx->worker_per_mach; i++){
      pthread_mutex_lock(&childs[i]->mu);
      if(!childs[i]->bgthreadcreated){
	rc = pthread_attr_init(&attr);
	checkResults("pthread_attr_init failed\n", rc);
	rc = pthread_create(&childs[i]->bgthid, &attr, worker_thread, childs[i]); 
	checkResults("pthread_create failed\n", rc);
	childs[i]->bgthreadcreated = true;       
      }
      // wake up threads that might be blocked
      if(childs[i]->queue.empty()){
	rc = pthread_cond_signal(&childs[i]->wakeupsignal);
      }

      iothread_item *item = (iothread_item *)calloc(1, sizeof(iothread_item));
      item->nbc = nbc;
      item->betaidval = betaidval;
      item->jobtype = j_residual;
      childs[i]->queue.push_back(item);

      strads_msg(INF, "in main scheduler allocate %p memchunk to (%d)th threads\n", item, i);
      pthread_mutex_unlock(&childs[i]->mu);
    }    

    // just do synchronization by checking if all workers have done residual update 
    for(int i=0; i < cctx->worker_per_mach; i++){
      while(1){
	pthread_mutex_lock(&childs[i]->mu);
	if(!childs[i]->retqueue.empty()){	
	  pthread_mutex_unlock(&childs[i]->mu);
	  iothread_ret *retitem = childs[i]->retqueue.front();
	  // process partial results from a worker.
	  // remove the entry from a worker in the queue 
	  childs[i]->retqueue.pop_front();	  
	  free(retitem);
	  break;
	}
	pthread_mutex_unlock(&childs[i]->mu);   	
      }
    }

    strads_msg(INF, "@@@@@@@@@@@@@@@@@  Mach(%d)rank finish residual update \n", tctx->rank);
    //    show_memusage();



    // Later .. remove this local beta data structure.. ///////////////////////////////////////////////////
    //          we do not need this at all. 
    // update my local beta with fresh beta values from the coordinator
    for(uint64_t i=0; i<nbc; i++){
      uint64_t tmpidx=betaidval[i].vid;
      double   newval=betaidval[i].beta;
      beta[tmpidx] = newval;
    }  

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    // MAKE MACHINE LEVEL SUM for parameters to update     
    uint64_t share;
    uint64_t progress=0;
    if(upc % cctx->worker_per_mach == 0){
      share=upc/cctx->worker_per_mach;   
    }else{
      share=upc/cctx->worker_per_mach + 1;   
    }

    for(int i=0; i < cctx->worker_per_mach; i++){
      pthread_mutex_lock(&childs[i]->mu);
      if(!childs[i]->bgthreadcreated){
	rc = pthread_attr_init(&attr);
	checkResults("pthread_attr_init failed\n", rc);
	rc = pthread_create(&childs[i]->bgthid, &attr, worker_thread, childs[i]); 
	checkResults("pthread_create failed\n", rc);
	childs[i]->bgthreadcreated = true;       
      }

      // wake up threads that might be blocked
      if(childs[i]->queue.empty()){
	rc = pthread_cond_signal(&childs[i]->wakeupsignal);
      }

      // assign a request from the coordinator to workers 
      // (divide the request by parameters...
      iothread_item *item = (iothread_item *)calloc(1, sizeof(iothread_item));
      for(uint64_t j=0; j<share; j++){
	if(progress < upc){
	  item->vidlist[j] = pvidlist[progress++];
	}else{
	  item->vidlist[j] = BIGLASSO_INVALID_VID;
	}	
      }
      item->size = share;
      item->jobtype = j_update;
      childs[i]->queue.push_back(item);

      strads_msg(INF, "in main scheduler allocate %p memchunk to (%d)th threads\n", item, i);
      pthread_mutex_unlock(&childs[i]->mu);
    }
    
    // collect work results     
    uint64_t resprogress=0;
    // BIG Serialization. 
    // TODO: Later change this.. 
    //       In that case, the assumption in biglasso.cpp receiving results part would be 
    //       broken. 
    for(int i=0; i < cctx->worker_per_mach; i++){
      while(1){
	pthread_mutex_lock(&childs[i]->mu);
	if(!childs[i]->retqueue.empty()){	
	  pthread_mutex_unlock(&childs[i]->mu);
	  iothread_ret *retitem = childs[i]->retqueue.front();
	  // process partial results from a worker.
	  // remove the entry from a worker in the queue 
	  childs[i]->retqueue.pop_front();	  
	  for(uint64_t k=0; k<retitem->size; k++){
	    if(retitem->idmpartialpair[k].vid != BIGLASSO_INVALID_VID){
	      idmp[resprogress].vid = retitem->idmpartialpair[k].vid;
	      idmp[resprogress].mpartial = retitem->idmpartialpair[k].mpartial;

	      idmp[resprogress].xsqmsum = retitem->idmpartialpair[k].xsqmsum;
	      
	      resprogress++;
	    }else{
	      //skip
	    }
	  }	  
	  free(retitem);
	  break;
	}
	pthread_mutex_unlock(&childs[i]->mu);   	
      }
    }// for(int i=0 ....

    assert(resprogress == upc);

    pos=0;
    c2smeta.upc = upc;
    c2smeta.c2s_signature = C2SSIGNATURE;
    memcpy(&scratch[pos], (uint8_t *)&c2smeta, sizeof(c2s_meta));
    pos += sizeof(c2s_meta);

    memcpy(&scratch[pos], (uint8_t *)idmp, upc*sizeof(idmpartial_pair));
    pos += sizeof(idmpartial_pair)*upc;

    end_meta endmeta;
    endmeta.end_signature = ENDSIGNATURE;
    memcpy(&scratch[pos], (uint8_t *)&endmeta, sizeof(end_meta));
    pos += sizeof(end_meta);

    *sendmsglen = pos;

    uint8_t *tmpbuf = (uint8_t *)calloc(pos, sizeof(uint8_t));
    memcpy(tmpbuf, scratch, pos);
    retpacket = tmpbuf;



    //    show_memusage();

  }


#if 1
  if(headp->type == s2c_objective){

    pos = sizeof(com_header);
    s2c_meta *ps2cmeta = (s2c_meta *)&recvbuf[pos];
    pos += sizeof(s2c_meta);

    uint64_t nbc = ps2cmeta->nbc;
    uint64_t upc = ps2cmeta->upc;
    assert(nbc == 0);
    assert(upc == 0);

    for(int i=0; i < cctx->worker_per_mach; i++){
      pthread_mutex_lock(&childs[i]->mu);
      if(!childs[i]->bgthreadcreated){
	rc = pthread_attr_init(&attr);
	checkResults("pthread_attr_init failed\n", rc);
	rc = pthread_create(&childs[i]->bgthid, &attr, worker_thread, childs[i]); 
	checkResults("pthread_create failed\n", rc);
	childs[i]->bgthreadcreated = true;       
      }
      // wake up threads that might be blocked
      if(childs[i]->queue.empty()){
	rc = pthread_cond_signal(&childs[i]->wakeupsignal);
      }

      iothread_item *item = (iothread_item *)calloc(1, sizeof(iothread_item));
      // no input is necessary for objective function calcluation except for beta vector.      
      item->jobtype = j_objective;
      childs[i]->queue.push_back(item);
      pthread_mutex_unlock(&childs[i]->mu);
    }    
    
    uint64_t resprogress=0;
    double obmsum=0;
    for(int i=0; i < cctx->worker_per_mach; i++){
      while(1){
	pthread_mutex_lock(&childs[i]->mu);
	if(!childs[i]->retqueue.empty()){	
	  pthread_mutex_unlock(&childs[i]->mu);
	  iothread_ret *retitem = childs[i]->retqueue.front();
	  childs[i]->retqueue.pop_front();	  
	  obmsum += retitem->obthsum;
	  free(retitem);
	  break;
	}
	pthread_mutex_unlock(&childs[i]->mu);   	
      }
    }// for(int i=0 ....

    assert(resprogress == upc);
    pos=0;
    c2smeta.upc = upc;
    c2smeta.c2s_signature = C2SSIGNATURE;

    memcpy(&scratch[pos], (uint8_t *)&c2smeta, sizeof(c2s_meta));
    pos += sizeof(c2s_meta);

    memcpy(&scratch[pos], (uint8_t *)&obmsum, sizeof(double));
    pos += sizeof(double);

    end_meta endmeta;
    endmeta.end_signature = ENDSIGNATURE;
    memcpy(&scratch[pos], (uint8_t *)&endmeta, sizeof(end_meta));
    pos += sizeof(end_meta);

    *sendmsglen = pos;
    uint8_t *tmpbuf = (uint8_t *)calloc(pos, sizeof(uint8_t));
    memcpy(tmpbuf, scratch, pos);
    retpacket = tmpbuf;

    //strads_msg(ERR, "OBJECTIVE VALUE CALC\n");

#if defined(SHOW_VM_PEAK)
    showVmPeak(tctx->rank);
#endif

  }
#endif 

  //////////////////////////
  // OOC FEATURE Scheduler-bg-app check whether a pending job is done or not. 
  // limitation: now, it can support only one pending job at a time. 
  //
  // check whether ioctx's pending job is done .... 
  pthread_mutex_lock(&ioctx->mu);
  if(!ioctx->retq.empty()){	
    pthread_mutex_unlock(&ioctx->mu);
    oocret_item *retitem = ioctx->retq.front();

    // free existing one. 
    // constraint: now, there is no working thread touching dp
    // //dpartitionctx->dp 

    // TODO : change this correctly

    //    tctx->awctx->dpart[0] = retitem->allocated_dpart; ///////////////////////////////////////////////////////////////
    ooc_ready_dp = retitem->allocated_dpart;
    //    delete(retitem->allocated_dpart);
    // THIS IS TENTATIVE CODE FOR TESTING (performance debugging).

    ioctx->retq.pop_front();	  
    free(retitem);
    strads_msg(ERR, "@@@@@@@@@@@@@@ MachRank(%d) finish OOC IO \n", tctx->rank);
    *oocdone = 1;

  }else{
    //    strads_msg(ERR, "@@@@@@@@@@@@@@ MachRank(%d) PENDING OOC IO not finished  \n", tctx->rank);
    pthread_mutex_unlock(&ioctx->mu);
  }

  //  assert(retpacket);
  return retpacket;
}
