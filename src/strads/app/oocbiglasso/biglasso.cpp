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
/* user defined code to implement a specific application 
   This file contains 
   1) foreground scheduler (user defined scheduler - workflow description) 
   2) worker that executes lasso update rule 
   3) miscalleneous 
      user defined print meta info 
      user defined configuration & meta info loading function - getuserconf */

#include <boost/crc.hpp>
#include <assert.h>
#include <queue>
#include <iostream>     // std::cout
#include <algorithm>    // std::next_permutation, std::sort
#include <vector>       // std::vector
#include <ctime>        // std::time
#include <cstdlib>      // std::rand, std::srand
#include <list>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <time.h>
#include <set>
#include "threadctx.hpp"
#include "basectx.hpp"
#include "utility.hpp"
#include "userdefined.hpp"
#include "strads-queue.hpp"
#include "scheduler-bg.hpp"
#include "usercom.hpp"
#include "getcorr.hpp"
#include "sched-delta.hpp"
#include "dpartitioner.hpp"
#include "iohandler.hpp"
#include "binaryio.hpp"

#define USER_DELIMITERS " =\n"
#define BIGLASSODISPLAYFRQ (100)

using namespace std;

idmpartial_pair gidmp[1024*1024]; // temporary working space 
deltaset gdeltalist;
sampleset gdset;
prev_beta prevbeta;

uint64_t _nzcount(double *beta, uint64_t length);
double _partialsum_update(dpartitionctx *dp, uint64_t vid, double *residual, double vidbeta, double *xsqmsum);
double _get_obj_partialsum(dpartitionctx *dp, uint64_t row_s, uint64_t row_end, double *yv, double *beta, uint64_t features);
void _init_memalloc_userctx_app(problemctx *pctx, appschedctx *appctx);
void _send_plan_wait_for_loading(threadctx *tctx, dpartitionctx *xdarray, uint64_t machines, uint64_t samples, uint64_t features);
void _makedpartplan_by_sample_for_workers(threadctx *tctx, dpartitionctx *xdarray, uint64_t partcnt, uint64_t samples, uint64_t features);

void _makedpartplan_by_column_for_schedulers(threadctx *tctx, dpartitionctx *xdarray, uint64_t partcnt, uint64_t samples, uint64_t features);

void _print_sampleset(sampleset *set);
void _send_plan_wait_for_loadingY(threadctx *tctx, uint64_t machines, uint64_t samples, uint64_t features);
void _send_plan_wait_for_loading_for_schedulers(threadctx *tctx, dpartitionctx *xdarray, uint64_t machines, uint64_t samples, uint64_t features);
double _soft_thrd(double sum, double lambda);
void _update_residual_by_sample(dpartitionctx *dp, double newB, double oldB, double *residual, uint64_t vid, uint64_t row_s, uint64_t row_end);

void _send_oocplan_no_wait_for_loading(threadctx *tctx, dpartitionctx *xdarray, uint64_t machines, uint64_t samples, uint64_t features);
void _send_oocplan_no_wait_for_loading_for_schedulers(threadctx *tctx, dpartitionctx *xdarray, uint64_t machines, uint64_t samples, uint64_t features);

double _get_obj_partialsum_with_residual(uint64_t row_s, uint64_t row_end, double *yv,  double *residual);

void _get_iterinfo(uint64_t oocpartitions,  uint64_t partcnt, double *beta, uint64_t betacnt, uint64_t *iterinfo, long oocfreq){


  strads_msg(ERR, "@@@@@@@@@@@ Get Iteration Information is called \n");

  uint64_t partsize = betacnt/partcnt;
  uint64_t oocnz, partnz;

  for(unsigned i=0; i<oocpartitions; i++){       
    oocnz = 0;
    for(unsigned j=0; j<partcnt; j++){
      if(j % oocpartitions == i){
	uint64_t start = partsize*j;
	uint64_t end = start + partsize -1;
	partnz=0;
	for(unsigned k=start; k<= end; k++){
	  //	  if(beta[k] != 0.0){
	  if(beta[k] >= 0.005){
	    partnz++;
	  }
	}
	oocnz += partnz;
	//strads_msg(ERR, " partno(%ld)(s:%ld e:%ld) of oocpartition(%d) nz: %ld\n", j, start, end, i, partnz);
      }
    }
    strads_msg(ERR, "\t\t OOC partition(%d) nz (>= 0.005): %ld\n", i,  oocnz);
    iterinfo[i] = oocnz;
  }

  uint64_t min;
  int minidx=0;
  min = iterinfo[0];
  
  for(unsigned int i=0; i<oocpartitions; i++){
    if(min > iterinfo[i]){
      minidx = i;
      min = iterinfo[i];
    }
  }

  strads_msg(ERR, "Min OOC GROUP: %d with %ld nz\n", minidx, min);
  double *tmp = (double *)calloc(oocpartitions, sizeof(double));
  for(unsigned int i=0; i<oocpartitions; i++){
    tmp[i] = iterinfo[i];    
  }

  for(unsigned int i=0; i<oocpartitions; i++){
    tmp[i] = (tmp[i]/min);
    // TEMPORAL TEST 
    tmp[i] = 1;

    if(tmp[i] > 15){
      tmp[i] = 15;
    }    
  }
  for(unsigned int i=0; i<oocpartitions; i++){
    tmp[i] = tmp[i]*oocfreq;
  }
  for(unsigned int i=0; i<oocpartitions; i++){
    iterinfo[i] = (uint64_t)tmp[i];
  }

  for(unsigned int i=0; i<oocpartitions; i++){
    strads_msg(ERR, "iterinfo[%d] =  [%ld] \n", i, iterinfo[i]);
  }

}


char *_get_lastoken(char *fn){
  char *ptr = strtok(fn, "/ \n");
  char *prev = NULL;
  while(ptr != NULL){
    prev = ptr;
    ptr = strtok(NULL, "/ \n");
  }  
  strads_msg(INF, "_get_lasttoken: %s\n", prev);
  return prev;
}

void _print_summary_ooc(uint64_t oocpartitions,  uint64_t partcnt, double *beta, uint64_t betacnt){

  uint64_t partsize = betacnt/partcnt;
  uint64_t oocnz, partnz;
  for(unsigned i=0; i<oocpartitions; i++){       
    oocnz = 0;
    for(unsigned j=0; j<partcnt; j++){
      if(j % oocpartitions == i){
	uint64_t start = partsize*j;
	uint64_t end = start + partsize -1;
	partnz=0;
	for(unsigned k=start; k<= end; k++){
	  if(beta[k] != 0.0){
	    partnz++;
	  }
	}
	oocnz += partnz;
	//strads_msg(ERR, " partno(%ld)(s:%ld e:%ld) of oocpartition(%d) nz: %ld\n", j, start, end, i, partnz);
      }
    }
    strads_msg(ERR, "\t\t OOC partition(%d) nz: %ld\n", i,  oocnz);
  }
}


char *_open_logfile(FILE **log, FILE **beta, uint64_t r, clusterctx *cctx, problemctx *pctx, experiments *expt){

  FILE *logfp, *betafp;
  char *yfile = pctx->yfile;
  char *algorithm = (char *)calloc(OUTFILENAME_MAX, sizeof(char));
  char *logfn  = (char *)calloc(OUTFILENAME_MAX, sizeof(char));
  char *betafn = (char *)calloc(OUTFILENAME_MAX, sizeof(char));
  char *tmpfn  = (char *)calloc(OUTFILENAME_MAX, sizeof(char));

  strcpy(tmpfn, yfile); // do not feed pctx->yfile(shared data) to strtok..
  char *ytoken = _get_lastoken(tmpfn);
  
#if defined(DELTA)	
#if defined (INTERFERENCE_CHECKING)
    sprintf(algorithm, "%s", "strads-ic-ooc");
#else
    sprintf(algorithm, "%s", "strads-nic-ooc");
#endif
#else
  sprintf(algorithm, "%s", "shotgun-ooc");
#endif
  
  sprintf(logfn, "%s%s_wmach_%d_wc_%d_schm_%d_sc_%d_r_%ld_lambda_%lf.log.%s.od%ld", 
	  expt->outputdir, ytoken, cctx->wmachines, cctx->totalworkers, cctx->schedmachines, 
	  cctx->totalschedthrd, r, expt->lambda, algorithm, expt->oocpartitions);

  sprintf(betafn, "%s%s_wmach_%d_wc_%d_schm_%d_sc_%d_r_%ld_lambda_%lf.beta.%s.od%ld", 
	  expt->outputdir, ytoken, cctx->wmachines, cctx->totalworkers, cctx->schedmachines, 
	  cctx->totalschedthrd, r, expt->lambda, algorithm, expt->oocpartitions);
	  
  logfp = (FILE *)fopen(logfn, "wt");
  betafp = (FILE *)fopen(betafn, "wt"); 

  assert(logfp);
  assert(betafp);

  *log = logfp;
  *beta = betafp;

  free(algorithm);
  //  free(logfn);
  free(betafn);
  free(tmpfn);

  return logfn;
}

void *userscheduler(void *arg){
  set<unsigned long>blockedthrds; // blocked scheduler threads 
  set<unsigned long>activethrds;  // active scheduler threads 
  set<unsigned long>pendingthrds;
  threadctx  *tctx=(threadctx *)arg;
  clusterctx *cctx=tctx->cctx;
  problemctx *pctx=tctx->pctx;
  appschedctx *appctx = tctx->mastctx->userctx;
  experiments *expt = appctx->expt;
  double *beta;
  uint8_t *recvbuf=NULL, *inbuf=NULL;
  com_header *headp=NULL;
  int recvlen=0;
  uint64_t features, samples, nz, partno, pos, batchsize=1;
  s2c_meta s2cmeta;
  end_meta endmeta;
  uint8_t *scratch = (uint8_t *)calloc(1024*1024, sizeof(uint8_t));
  uint64_t *tmpvids = (uint64_t *)calloc(MAX_BETA_TO_UPDATE, sizeof(uint64_t));
  uint64_t iteration=0, sentupc;
  uint64_t stime, etime, objtimesum=0;
  bool init_stage=true;
  //  std::list<double>tmpdeltalist;
  dataset_t deltamap;
  value_index_t& deltamap_index = deltamap.get<value_tag>();
  double lambda = expt->lambda;
  double newminunit = expt->initminunit, oldminunit;
  uint64_t scanwindow = expt->scanwindow;
  uint64_t dfrequency = expt->dfrequency;
  double dpercentile = expt->dpercentile;
  dpartitionctx *xdarray= (dpartitionctx *)calloc(cctx->wmachines, sizeof(dpartitionctx));
  dpartitionctx *xdarray_scheduler= (dpartitionctx *)calloc(cctx->schedmachines-1, sizeof(dpartitionctx));
  uint64_t init_touch=0;
  FILE *logfp, *betafp;
  char *logfn = _open_logfile(&logfp, &betafp, expt->desired, cctx, pctx, expt);
  int totalschedthrds = cctx->schedthrd_per_mach*(cctx->schedmachines-1);
  int oocpartitions = expt->oocpartitions;
  int cpoint = 0;
  bool oocswitchingflag = false;
  uint64_t oocmodulo_tmp = 0;
  std::set<int> ooc_wpending;
  std::set<int> ooc_wdone;
  bool wpending = false;
  int cmodulo;
  long oociter = 0;
  uint64_t *iterinfo = (uint64_t *)calloc(oocpartitions, sizeof(uint64_t));
  double init_sweepoutrate = expt->init_sweepoutrate;
#if !defined (BINARY_IO_FORMAT)
  iohandler_spmat_mmio_read_size(pctx->xfile, &samples, &features, &nz);
#else
  iohandler_spmat_pbfio_read_size(pctx->xfile, &samples, &features, &nz);
#endif
  // AT this point, 
  // # of features shold be known
  uint64_t init_min_touch = features*init_sweepoutrate;
  strads_msg(ERR, "@ Coordinator: OOC partitions: %d ... init_sweepoutrate: %lf .. init_min_touch: %ld\n", 
	     oocpartitions, init_sweepoutrate, init_min_touch);
  _init_memalloc_userctx_app(pctx, appctx);
  beta = (double *)calloc(features, sizeof(double));
  assert(beta);
  strads_msg(ERR, "\t[App-Scheduler] App main coordinator boots up in rank (%d) timelimit (%lf) sec\n", 
	     tctx->rank, expt->timelimit);
  strads_msg(ERR, "\t\tstrads input file(%s) has %ld samples, %ld features nzcnt %ld\n", 
	     pctx->xfile, samples, features, nz);
  strads_msg(ERR, "\tSend dpartition cmd to machines\n");
  // send this info only for worker machines... 
  _makedpartplan_by_sample_for_workers(tctx, xdarray, cctx->wmachines, samples, features);  
  xdarray->range.ooc_hmodflag = 1;
  xdarray->range.h_modulo = oocmodulo_tmp;
  xdarray->range.oocdpartitions = oocpartitions;
  //  oocmodulo_tmp = (oocmodulo_tmp+1)%oocpartitions;   
  // TODO: make a plan for scheduler  here 

  strads_msg(OUT, "[coordinator] send data loading command to wrker and scheduler machines\n");
  _send_plan_wait_for_loading(tctx, xdarray, cctx->wmachines, samples, features);
  // TODO: replace cctx->wmachines with right numbers 
  sleep(10);
  _send_plan_wait_for_loadingY(tctx, cctx->wmachines, samples, 1);
  strads_msg(OUT, "[coordinator] all workers have done loading a partitioned data\n");

#if defined(INTERFERENCE_CHECKING)
  _makedpartplan_by_column_for_schedulers(tctx, xdarray_scheduler, cctx->schedmachines-1, samples, features);  
  // TODO xdarray_scheduler-> .. 
  // move oocmodulo_tmp = (oocmodule_tmp +1) to here
  xdarray_scheduler->range.ooc_hmodflag = 1;
  xdarray_scheduler->range.h_modulo = oocmodulo_tmp;
  xdarray_scheduler->range.oocdpartitions = oocpartitions;
  oocmodulo_tmp = (oocmodulo_tmp+1)%oocpartitions;   
  _send_plan_wait_for_loading_for_schedulers(tctx, xdarray_scheduler, cctx->schedmachines-1, samples, features);
  // TODO: replace cctx->wmachines with right numbers 
  strads_msg(OUT, "[coordinator] all scheduler have done loading a partitioned data\n");
#endif 

  psched_start_scheduling(NULL,  tctx, 0.1);
  strads_msg(ERR, "@@@@@@@@@@@@@ STRADS APP send trigger message to all schedulers \n");
  // OOC FEATURE
  xdarray->range.ooc_hmodflag = 1;
  xdarray->range.h_modulo = oocmodulo_tmp;
  xdarray->range.oocdpartitions = oocpartitions;
  xdarray_scheduler->range.ooc_hmodflag = 1;
  xdarray_scheduler->range.h_modulo = oocmodulo_tmp;
  xdarray_scheduler->range.oocdpartitions = oocpartitions;
  oocmodulo_tmp = (oocmodulo_tmp+1)%oocpartitions;   
  assert(cctx->totalmachines == (cctx->schedmachines + cctx->wmachines));

  for(int m=0; m < cctx->totalmachines-1; m++){
    ooc_wpending.insert(m);
  }

  wpending = true;
  _send_oocplan_no_wait_for_loading(tctx, xdarray, cctx->wmachines, samples, features);
  strads_msg(ERR, "A-COORDINATOR SEND OOC CMD TO THE WORKERS: MODULO (%ld) wpendingsize(%ld)\n", 
	     xdarray->range.h_modulo, ooc_wpending.size());
  _send_oocplan_no_wait_for_loading_for_schedulers(tctx, xdarray_scheduler, cctx->schedmachines-1, samples, features);
  prevbeta.betacnt = 0;
  stime = timenow();
  for(int i=0; i< cctx->schedthrd_per_mach*(cctx->schedmachines-1); i++){
    blockedthrds.insert(i);
  }

  for(int i=0; i< cctx->schedthrd_per_mach*(cctx->schedmachines-1); i++){
    if(i % oocpartitions == cpoint){  // TODO : 
      psched_send_blockcmd(tctx, i, wake_up, 0);   // wake up even only 
      if(blockedthrds.find(i) == blockedthrds.end()){
	strads_msg(ERR, "Fatal: tries to wake up non blocked one P1 partno(%d)\n", i);
	exit(0);
      }else{
	blockedthrds.erase(i);
	activethrds.insert(i);
      }
    }
  }
  cpoint = cpoint ++;
  cpoint = cpoint % oocpartitions;

  strads_msg(OUT, "[coordinator] start lasso iterative update \n\n");
  int cur_cpoint; 
  while(1){   // BIG LOOP 
    recvbuf = NULL;    
    recvlen = syscom_async_recv_amo_malloc_comhandler(tctx->comh[SGR], &recvbuf);
    if(recvlen > 0){      
      headp = (com_header *)recvbuf;
      if(headp->type == indsetdispatch){
	if(headp->oocdone == 1){
	  strads_msg(ERR, "\n\n[ooc cmd] from %d scheduler, OOC done msg arrives \n\n", 
		     headp->src_rank);	      
	  assert(wpending == true);
	  if(ooc_wpending.find(headp->src_rank) != ooc_wpending.end()){
	    strads_msg(ERR, "OOC wpending size before erase (%ld) .. delete %d \n", 
		       ooc_wpending.size(), headp->src_rank);
	    ooc_wpending.erase(headp->src_rank);
	    ooc_wdone.insert(headp->src_rank);
	  }else{
	    strads_msg(ERR, "\n\n@@@@ SOME THING WRONG: %d is not in the wpending set \n\n", headp->src_rank);
	    assert(0);
	    exit(0);
	  }
	  //	  if(ooc_wdone.size() == cctx->wmachines){	    
	  if(ooc_wdone.size() == cctx->totalmachines-1){	    
	    assert(0); // this can not happen in normal case...
	    strads_msg(ERR, "\n\nall machines are io loading done with last scheduler done \n");	  
	    wpending = false;		  
	    assert(ooc_wpending.size() == 0);
	  }
	}// headp->oocdone == 1

	partno = psched_get_samples(recvbuf, &gdset);	
	if(cpoint == 0){
	  cur_cpoint = oocpartitions-1; 
	}else{
	  cur_cpoint = cpoint-1;
	}

	if(partno % oocpartitions != cur_cpoint){
	  strads_msg(ERR, "cpoint (%d) ignore partion (%ld) \n", cpoint, partno);
	  continue;
	}

	oociter++;
	strads_msg(INF, "\t\t[coordinator] receive a round from part(%ld) size(%ld)\n", partno, gdset.size);
	pos = 0;
	s2cmeta.nbc = prevbeta.betacnt;
	s2cmeta.upc = gdset.size;	
	sentupc = s2cmeta.upc;
	s2cmeta.s2c_signature = S2CSIGNATURE;
	init_touch += gdset.size;
	memcpy(&scratch[pos], (uint8_t *)&s2cmeta, sizeof(s2c_meta));
	pos += sizeof(s2c_meta);
	memcpy(&scratch[pos], (uint8_t*)prevbeta.id_val_pair, prevbeta.betacnt*sizeof(idval_pair));
	pos += prevbeta.betacnt*sizeof(idval_pair);
	for(uint64_t j=0; j<s2cmeta.upc; j++){
	  tmpvids[j] = gdset.samples[j];
	}
	memcpy(&scratch[pos], (uint8_t *)tmpvids, s2cmeta.upc*sizeof(uint64_t));
	pos +=s2cmeta.upc*sizeof(uint64_t);
	endmeta.end_signature = ENDSIGNATURE;
	memcpy(&scratch[pos], (uint8_t *)&endmeta, sizeof(end_meta));
	pos += sizeof(end_meta);
	// send a round to the worker machines 
	for(uint64_t i=0; i < (long unsigned int)cctx->wmachines; i++){
	  syscom_send_amo_comhandler(tctx->comh[WGR], tctx, (uint8_t *)scratch, pos, s2c_update, i, partno);	  
	}
	// receive results from workers. 	
	for(int i=0; i < cctx->wmachines; i++){
	  // get one results from one worker machine	 
	  while(1){
	    inbuf = NULL;	    
	    recvlen = syscom_async_recv_amo_malloc_comhandler(tctx->comh[WGR], &inbuf);
	    if(recvlen > 0){
	      assert(inbuf);
	      headp = (com_header *)inbuf;
	      if(headp->oocdone == 1){
		strads_msg(ERR, "\n\n[ooc cmd]from %d worker mach, OOC done msg arrives \n\n", 
			   headp->src_rank);	      
		assert(wpending == true);
		if(ooc_wpending.find(headp->src_rank) != ooc_wpending.end()){
		  strads_msg(ERR, "OOC WPENDING.ERASE beforesize(%ld) .. delete %d \n", 
			     ooc_wpending.size(), headp->src_rank);
		  ooc_wpending.erase(headp->src_rank);
		  ooc_wdone.insert(headp->src_rank);
		}else{
		  strads_msg(ERR, "\n\nfatal some thing wrong: %d is not in the wpending set \n\n", headp->src_rank);
		  assert(0);
		  exit(0);
		}
		//		if(ooc_wdone.size() == cctx->wmachines){
		if(ooc_wdone.size() == cctx->totalmachines-1){
		  strads_msg(ERR, "\n\n[ooc cmd] all machines are io loading done \n");
		  wpending = false;		  
		  assert(ooc_wpending.size() == 0);
		}
	      }// headp->oocdone == 1 
	      uint64_t pos =0;
	      if(headp->type == c2s_result){
		pos += sizeof(com_header);
		c2s_meta *pc2smeta = (c2s_meta *)&inbuf[pos];
		assert(pc2smeta->c2s_signature == C2SSIGNATURE);
		pos += sizeof(c2s_meta);
		idmpartial_pair *idmp = (idmpartial_pair *)&inbuf[pos];
		pos += pc2smeta->upc*sizeof(idmpartial_pair);
		end_meta *pendmeta = (end_meta *)&inbuf[pos];
		assert(pendmeta->end_signature == ENDSIGNATURE);
		assert(sentupc == pc2smeta->upc);
		for(uint64_t j=0; j < pc2smeta->upc; j++){
		  assert(idmp[j].vid != BIGLASSO_INVALID_VID);
		  strads_msg(INF, "\t\t from rank(%d) vid(%ld) mpartial(%lf)\n", 
			     headp->src_rank, idmp[j].vid, idmp[j].mpartial);
		  // BIG ASSUMPTION:
		  // VIDS in the packet are sorted in the client thread side. 
		  if(i == 0){
		    gidmp[j].vid = idmp[j].vid;
		  }else{
		    assert(gidmp[j].vid == idmp[j].vid);
		  }
		  gidmp[j].mpartial += idmp[j].mpartial;
		  gidmp[j].xsqmsum += idmp[j].xsqmsum;
		}
		free(inbuf);
		break;
	      }	   
	    }
	  }// while(1)...   
	} // for(int i=0; ....
	uint64_t bprogress=0;
	uint64_t prebetaprogress=0;
	for(uint64_t i=0; i<sentupc; i++){
	  uint64_t tmpvid = gidmp[i].vid;
	  assert(tmpvid != BIGLASSO_INVALID_VID);
	  double newbeta;
#if !defined(INTERCEPT_COL_CARE)
	  newbeta = _soft_thrd(gidmp[i].mpartial, lambda);
#else
	  if(tmpvid == pctx->features-1){ // intercept col, set lambda to zero 
	    newbeta = _soft_thrd(gidmp[i].mpartial, 0.0);
	    strads_msg(INF, "I got last col for intercept beta: %lf\n", newbeta);
	  }else{	    
	    newbeta = _soft_thrd(gidmp[i].mpartial, lambda);
	  }
	  assert(0); // this else routine is invalid.
#endif
	  newbeta = newbeta/gidmp[i].xsqmsum;


	  if(newbeta != beta[tmpvid]){
	    prevbeta.id_val_pair[prebetaprogress].vid = tmpvid;
	    prevbeta.id_val_pair[prebetaprogress].beta = newbeta;
	    prevbeta.id_val_pair[prebetaprogress].oldbeta = beta[tmpvid];	  
	    prebetaprogress++;
	  }

	  gdeltalist.idxlist[bprogress] = tmpvid;
	  gdeltalist.deltalist[bprogress]= fabs(newbeta - beta[tmpvid]);
	  beta[tmpvid] = newbeta;
	  bprogress++;
	  strads_msg(INF, " (%ld) vid msum %lf  newbeta(%lf)\n", gidmp[i].vid, gidmp[i].mpartial, newbeta);
	}
	gdeltalist.size = bprogress ;
	prevbeta.betacnt = prebetaprogress;
	
	for(uint64_t i=0; i<prevbeta.betacnt; i++){	  
	  strads_msg(INF, "@@ CCoordinator PREVBETA vid(%ld)  newbeta(%lf) oldbeta(%lf) betacnt(%ld) iteration(%ld)\n", 
		     prevbeta.id_val_pair[i].vid, prevbeta.id_val_pair[i].beta, prevbeta.id_val_pair[i].oldbeta, 
		     prevbeta.betacnt, iteration); 
	}      
	// initialize gidmp for next sum operation. -- this is important. 
	for(uint64_t i=0; i<sentupc+1000; i++){
	  gidmp[i].mpartial=0; 	  
	  gidmp[i].xsqmsum=0; 	  
	}
	
#if defined(DELTA)	
	if(init_stage){	
	  if(init_touch > (init_min_touch)){
	    init_stage = false;
	    gdeltalist.size=0;
	    for(uint64_t j=0; j < features; j++){
	      if(beta[j] != 0.0){
		gdeltalist.idxlist[gdeltalist.size] = j;
		gdeltalist.deltalist[gdeltalist.size] = beta[j]; 
		gdeltalist.size++;		
	      }
	    }	    
	    strads_msg(OUT, "\n[Coordinator] Initial step is done. Summary: %ld nz touched var: %ld\n", 
		       gdeltalist.size, init_touch);
	    strads_msg(OUT, "[Coordinator] Dynamic scheduling starts!\n\n");
	    psched_process_delta_update(&gdeltalist,  tctx, 0.1, expt->afterdesired);
	  }else{	  
	    // follow system configuration 
	    if( wpending == false && (iteration != 0) && (oociter > expt->oocfreq)){
	      strads_msg(ERR, "########################### SWITCHING ON P1\n");
	      oocswitchingflag = true;
	      wpending = true;
	    }else{
	      strads_msg(INF, "iteration: %ld  oociter : %ld oocfreq(%ld) switchingflag(%d) wpending(%d) wpending.size(%ld)\n", 
			 iteration, oociter, expt->oocfreq, oocswitchingflag, wpending, ooc_wpending.size()); 
	    }
	   
	    if(oocswitchingflag == true){	     
	      if(activethrds.find(partno) != activethrds.end()){
		strads_msg(ERR, "send blocking message to partno %ld \n", partno);
		psched_send_blockcmd(tctx, partno, deltaupdate, 1); // block the partno threads 
		//delta from latest iteration is ignored. this is fine for init-stage. 
		pendingthrds.insert(partno);
		activethrds.erase(partno);
	      }else{
		strads_msg(ERR, "Something wrong: partno(%ld) is not in active list.....\n", partno);
	      }
	      strads_msg(ERR, "Active Size (%ld)\n", activethrds.size());

	      if(activethrds.size() == 0){
		std::set<unsigned long>::iterator pit;
		unsigned long ptmp;
		for(pit = pendingthrds.begin(); pit != pendingthrds.end(); pit++){
		  ptmp = *pit;
		  blockedthrds.insert(ptmp);
		}

		pendingthrds.erase(pendingthrds.begin(), pendingthrds.end());
		// OOC FEATURE
		xdarray->range.ooc_hmodflag = 1;
		xdarray->range.h_modulo = oocmodulo_tmp;
		xdarray->range.oocdpartitions = oocpartitions;
		xdarray_scheduler->range.ooc_hmodflag = 1;
		xdarray_scheduler->range.h_modulo = oocmodulo_tmp;
		xdarray_scheduler->range.oocdpartitions = oocpartitions;
		oocmodulo_tmp = (oocmodulo_tmp+1)%oocpartitions;   
		// only for worker machines
		//		for(int m=0; m < cctx->wmachines; m++){
		for(int m=0; m < cctx->totalmachines-1; m++){		  
		  ooc_wpending.insert(m);
		}
		ooc_wdone.erase(ooc_wdone.begin(), ooc_wdone.end());
		wpending = true;
		_send_oocplan_no_wait_for_loading(tctx, xdarray, cctx->wmachines, samples, features);
		//		strads_msg(ERR, "COORDINATOR SEND OOC CMD TO THE WORKERS: MODULO (%ld)\n", xdarray->range.h_modulo);
		_send_oocplan_no_wait_for_loading_for_schedulers(tctx, xdarray_scheduler, cctx->schedmachines-1, samples, features);
		strads_msg(ERR, "COORDINATOR SEND OOC CMD TO THE WORKERS: MODULO (%ld) -- pending size(%ld)\n", 
			   xdarray->range.h_modulo, ooc_wpending.size());
		oociter = 0;

		// BLOCK A above is moved here for debugging purpose		
		for(int i=0; i< cctx->schedthrd_per_mach*(cctx->schedmachines-1); i++){
		  if(i % oocpartitions == cpoint){  // TODO : 
		    strads_msg(ERR, "Wakt Up from Server to partno(%d)\n", i);
		    psched_send_blockcmd(tctx, i, wake_up, 0);   // wake up the i-th partno thread  
		    if(blockedthrds.find(i) == blockedthrds.end()){
		      strads_msg(ERR, "Fatal: tries to wake up non blocked one P2 partno(%d)\n", i);
		      exit(0);
		    }else{
		      blockedthrds.erase(i);
		      activethrds.insert(i);
		    }
		  }
		}
		cpoint = cpoint ++;
		cpoint = cpoint % oocpartitions;
		strads_msg(ERR, "@@@@@ INITSTAGE: CPOINT (%d) for next iteration\n", cpoint); 
		oocswitchingflag = false;		
	      } // if(activethrds.size() == 0	      	     	      	     
	    }else{ // if(oocswitchingflag == true)
	      psched_send_token(tctx, partno);	    
	    }   // if(oocswitchingflag == true)
	  } //  if(init_touch > (init_min_touch))
	}else{ // if initstate ......  after initializattion, apply adaptive min unit policy
	  for(uint64_t t=0; t < gdeltalist.size; t++){
	    strads_msg(INF, " [%ld : %lf]\n", gdeltalist.idxlist[t], gdeltalist.deltalist[t]);
	    if(gdeltalist.deltalist[t] != 0.0) // keep non zero only 
	      deltamap.insert(element_t(gdeltalist.idxlist[t], gdeltalist.deltalist[t]));
	  }	 
#if 1	 
	  if(iteration % scanwindow == 0){
	    uint64_t stoppos;  
	    if(deltamap.size() > 20){
	      stoppos = deltamap.size()*(dpercentile);
	      assert(stoppos > 0);
	      assert(stoppos < deltamap.size());
	    }else{
	      // THIS IS SPECIAL REMEDY to correct skewed nz distribution from OOC. 
	      if(deltamap.size() <= 2){	       
		deltamap.insert(element_t(1, 0.0000001));
		deltamap.insert(element_t(2, 0.0000001));
		deltamap.insert(element_t(3, 0.0000001));
		strads_msg(ERR, "Sadly, too small deltamap, add three more entries. deltamap.size(%ld)\n", 
			   deltamap.size());

	      }
	      // TODO: When you resolve the skewed problem, remove this if(deltamp.size <= 2 ... block 
	      assert(deltamap.size() > 2);	      
	      if(deltamap.size() < 2){
		strads_msg(ERR, "No expectation for further progress. terminate strads\n");
		break;
	      }
	      stoppos = deltamap.size() - 1;
	    }
	    strads_msg(INF, "@@@@@@@ deltamap size [%ld] STOPPOS %ld (dpercentile: %lf)\n",
		       deltamap.size(), stoppos, dpercentile);
	    // deltamap has ascending order.
	    uint64_t scanprogress=0;
	    value_index_t::iterator minit = deltamap_index.begin();
	    assert(minit != deltamap_index.end());
	    oldminunit = newminunit;
	    for(; minit != deltamap_index.end(); minit++){
	      strads_msg(INF, "deltaMap prog(%ld) (%ld:%lf)\n", scanprogress, minit->first, minit->second);
	      newminunit = minit->second;
	      if(++scanprogress == stoppos){
		break;
	      }
	    }
	    uint64_t deltasize = deltamap.size();
	    value_index_t::reverse_iterator mita = deltamap_index.rbegin();
	    double maxdeltaval = mita->second;	    
	    deltamap.erase(deltamap.begin(), deltamap.end());      
	    if(newminunit < 0.0000001){
	      newminunit =  0.0000001;
	    }
#if defined(FIXED_MIN_UNIT)
	    newminunit =  0.0000001;
#endif
	    strads_msg(INF, "\tMin Unit Change (%lf) --> (%lf) info. deltamapsize(%ld) maxdelta(%2.20lf)\n", 
		       oldminunit, newminunit, deltasize, maxdeltaval);
	  }
#endif 
	  if( wpending == false && (iteration != 0) && (oociter > expt->oocfreq)){
	    strads_msg(ERR, "################################# SWITCHING ON P2  from(%d)(finish %ld) to (%d) \n", 
		       cpoint, iterinfo[cur_cpoint], (cpoint+1)%oocpartitions);	   	   
	    oocswitchingflag = true;
	    wpending = true;
	    oociter=0;	      
	  }

	  if(oocswitchingflag == true){	     
	    strads_msg(INF, "#################################### Active partno: %ld \n", partno);
	    if(activethrds.find(partno) != activethrds.end()){
	      strads_msg(ERR, "Point 2 after initialization Send Blocking Msg to %ld part scheduler thread\n", partno);
#if defined(DELTA_UPDATE_OPTIMIZATION)
	      psched_process_delta_update_with_partno(&gdeltalist,  tctx, newminunit, expt->afterdesired, partno, 1);	      
#else
	      psched_process_delta_update_onepartition(&gdeltalist,  tctx, newminunit, expt->afterdesired, partno, 1);
#endif
	      // last parameter 1: cmd to block the dest thread
	      pendingthrds.insert(partno);
	      activethrds.erase(partno);
	    }else{
	      strads_msg(ERR, "Point 2 after initialization Something wrong: partno(%ld) is not in active list.....\n", partno);
	    }

	    if(activethrds.size() == 0){	    
	      std::set<unsigned long>::iterator pit;
	      unsigned long ptmp;
	      for(pit = pendingthrds.begin(); pit != pendingthrds.end(); pit++){
		ptmp = *pit;
		blockedthrds.insert(ptmp);
	      }
	      pendingthrds.erase(pendingthrds.begin(), pendingthrds.end());
#if 1
	      // OOC FEATURE
	      xdarray->range.ooc_hmodflag = 1;
	      xdarray->range.h_modulo = oocmodulo_tmp;
	      xdarray->range.oocdpartitions = oocpartitions;
	      xdarray_scheduler->range.ooc_hmodflag = 1;
	      xdarray_scheduler->range.h_modulo = oocmodulo_tmp;
	      xdarray_scheduler->range.oocdpartitions = oocpartitions;	      
	      oocmodulo_tmp = (oocmodulo_tmp+1)%oocpartitions;   
	      for(int m=0; m < cctx->totalmachines-1; m++){
		ooc_wpending.insert(m);
	      }
	      ooc_wdone.erase(ooc_wdone.begin(), ooc_wdone.end());
	      wpending = true;
	      _send_oocplan_no_wait_for_loading(tctx, xdarray, cctx->wmachines, samples, features);
	      _send_oocplan_no_wait_for_loading_for_schedulers(tctx, xdarray_scheduler, cctx->schedmachines-1, samples, features);
#if 1
	      for(int i=0; i< cctx->schedthrd_per_mach*(cctx->schedmachines-1); i++){
		if(i % oocpartitions == cpoint){  // TODO : 
		  psched_send_blockcmd(tctx, i, wake_up, 0);   // wake up even only 

		  if(blockedthrds.find(i) == blockedthrds.end()){
		    strads_msg(ERR, "Fatal: tries to wake up non blocked one P3 partno(%d)\n", i);
		    assert(0);
		    exit(0);
		  }else{
		    blockedthrds.erase(i);
		    activethrds.insert(i);
		  }
		}
	      }
	      cpoint = cpoint ++;
	      cpoint = cpoint % oocpartitions;
	      oocswitchingflag = false;
	      _get_iterinfo(oocpartitions,(cctx->schedmachines-1)*cctx->schedthrd_per_mach, beta,features,iterinfo,expt->oocfreq);
#endif 
	      strads_msg(ERR, "COORDINATOR SEND OOC CMD TO THE WORKERS: MODULO (%ld) -- pending size(%ld)\n", 
			 xdarray->range.h_modulo, ooc_wpending.size());
#endif
	      oociter = 0;
	    } // if(activethrds.size() == 0	      	     	      	     
	  }else{ // if(oocswitchingflag == true)
#if defined(DELTA_UPDATE_OPTIMIZATION)	    
	    psched_process_delta_update_with_partno(&gdeltalist,  tctx, newminunit, expt->afterdesired, partno, 0);
#else
	    psched_process_delta_update_onepartition(&gdeltalist,  tctx, newminunit, expt->afterdesired, partno, 0);
#endif
	  }
	}
#else
	psched_send_token(tctx, partno);
#endif 
      } // if headp->type == indsetdispatch .... 
      free(recvbuf); // free buffer for a round .... 
      recvbuf = NULL;
      if( (++iteration % dfrequency) == 0){
	double objstime = timenow();
	/////////////////////////////////////////////////////////// OBJECT FUNCTION CALC 
	pos = 0;
	s2cmeta.nbc = 0;
	s2cmeta.upc = 0;	
	sentupc = s2cmeta.upc;
	s2cmeta.s2c_signature = S2CSIGNATURE;
	memcpy(&scratch[pos], (uint8_t *)&s2cmeta, sizeof(s2c_meta));
	pos += sizeof(s2c_meta);
	endmeta.end_signature = ENDSIGNATURE;
	memcpy(&scratch[pos], (uint8_t *)&endmeta, sizeof(end_meta));
	pos += sizeof(end_meta);
	// send a round to the worker machines 
	for(uint64_t i=0; i < (long unsigned int)cctx->wmachines; i++){
	  syscom_send_amo_comhandler(tctx->comh[WGR], tctx, (uint8_t *)scratch, pos, s2c_objective, i, partno);	  
	}
	double pterm=0;
	for(uint64_t i=0; i<features; i++){
	  pterm += fabs(beta[i]);	 
	}
	uint64_t nzcnt = _nzcount(beta, features);
	double objsum=0;
	for(int i=0; i < cctx->wmachines; i++){
	  // get one results from one worker machine	 
	  while(1){
	    inbuf = NULL;	    
	    recvlen = syscom_async_recv_amo_malloc_comhandler(tctx->comh[WGR], &inbuf);
	    if(recvlen > 0){
	      assert(inbuf);
	      headp = (com_header *)inbuf;
	      uint64_t pos =0;
	      if(headp->oocdone == 1){
		strads_msg(ERR, "\n\n@@@@@@@@@@@@@@@@ OBJ Special From %d mach, OOC done msg arrives \n\n", 
			   headp->src_rank);	      
		assert(wpending == true);
		if(ooc_wpending.find(headp->src_rank) != ooc_wpending.end()){
		  strads_msg(ERR, "Special OBJ OOC WPENDING.ERASE beforesize(%ld) .. delete %d \n", 
			     ooc_wpending.size(), headp->src_rank);
		  ooc_wpending.erase(headp->src_rank);
		  ooc_wdone.insert(headp->src_rank);
		}else{
		  strads_msg(ERR, "\n\n@@@@ OBJ Special SOME THING WRONG: %d is not in the wpending set \n\n", headp->src_rank);
		  assert(0);
		  exit(0);
		}
		//		if(ooc_wdone.size() == cctx->wmachines){
		if(ooc_wdone.size() == cctx->totalmachines-1){
		  wpending = false;		  
		  assert(ooc_wpending.size() == 0);
		}
	      }// headp->oocdone == 1 
	      if(headp->type == c2s_result){
		pos += sizeof(com_header);
		c2s_meta *pc2smeta = (c2s_meta *)&inbuf[pos];
		assert(pc2smeta->c2s_signature == C2SSIGNATURE);
		pos += sizeof(c2s_meta);
		double *pmsum = (double *)&inbuf[pos];
		pos += sizeof(double);
		end_meta *pendmeta = (end_meta *)&inbuf[pos];
		assert(pendmeta->end_signature == ENDSIGNATURE);
		objsum += (*pmsum);
		free(inbuf);
		break;
	      } // headp->type == c2s_result
	    }
	  }// while(1)...   
	} // for(int i=0; i < cctx->wmachines; i++ ... 
	double objetime = timenow();
	etime = timenow();	
	objtimesum += (objetime-objstime);
	strads_msg(OUT, "iteration %ld obj: %10.10lf nz: %ld  elapsed: %lf sec objt(%lf)-compt(%lf) updated params(%ld)\n", 
		   iteration, objsum + pterm*lambda, nzcnt, 
		   (etime-stime)/1000000.0, (objetime-objstime)/1000000.0, 
		   (etime-stime - objtimesum)/1000000.0, init_touch);
	fprintf(logfp, " %ld  %10.10lf  %ld %lf %lf %lf %ld\n", 
		   iteration, objsum + pterm*lambda, nzcnt, 
		   (etime-stime)/1000000.0, (objetime-objstime)/1000000.0, 
		   (etime-stime - objtimesum)/1000000.0, init_touch);
	fflush(logfp);

#if defined(PRINT_SUMMARY_OOCINFO)
	_print_summary_ooc(oocpartitions, (cctx->schedmachines-1)*cctx->schedthrd_per_mach, beta, features);
#endif
	if(expt->timelimit < (etime-stime)/1000000.0){
	  strads_msg(ERR, " reach max time limit (%lf) second \n", expt->timelimit);
	  break;
	}       
      }// if(iteration == 100 ) .......

      if(iteration == expt->maxiter){
	strads_msg(ERR, " reach max iteration limit (%d) \n", expt->maxiter);
	break;
      }

    }// if(recvlen > 0 ...         

  }// while 1 
  
  for(uint64_t i=0; i < features; i++){
    if(beta[i] == 0.0)
      continue;
    fprintf(betafp, "%ld %lf\n", i, beta[i]);
  }

  fclose(logfp);
  fclose(betafp);
  strads_msg(OUT, "good-bye normal termination.\nSee %s for log\n", logfn);   
  exit(0);
  return NULL;
}


void init_worker_threadctx(wbgctx *ctx, threadctx *refthreadctx, int lid){

  pthread_mutex_init(&ctx->mu, NULL);
  pthread_cond_init(&ctx->wakeupsignal, NULL);

  ctx->bgthreadcreated = false;
  ctx->ctxinit = true;
  ctx->mthreadctx = refthreadctx;
  ctx->cctx = refthreadctx->cctx;
  ctx->lid = lid;
  ctx->rank = refthreadctx->rank;

  assert(refthreadctx->pctx);
  ctx->pctx = refthreadctx->pctx;
  assert(refthreadctx->awctx);
  assert(refthreadctx->awctx->expt);	
  ctx->expt = refthreadctx->awctx->expt;
  ctx->awctx = refthreadctx->awctx;
  // refthreadctx is the machine thread...
}


void init_client_thread(threadctx *tctx, uint64_t samples, uint64_t features){
  strads_msg(INF, "@@@@@ Rank(%d)  alloc beta, residual with Samples(%ld) Features(%ld)\n", 
	     tctx->rank, samples, features);
  tctx->awctx->samples = samples;
  tctx->awctx->features = features;
  tctx->awctx->beta = (double *)calloc(features, sizeof(double));
  tctx->awctx->residual = (double *)calloc(samples, sizeof(double));
  tctx->awctx->yvector = (double *)calloc(samples, sizeof(double));
  assert(tctx->awctx->beta);
}


/* this function will read input configuration file and parse the file, 
   then fill out user defined application specific data structure */
void getuserconf(experiments *expt, clusterctx *cctx, int rank){

  if(rank == 0)
    strads_msg(ERR, "mt-getuserconf: config file name %s\n", expt->userconfigfile);

  char param[256], *ptr;
  char *linebuf = (char *)calloc(IOHANDLER_BUFLINE_MAX, sizeof(char));
  assert(linebuf != NULL);  
  FILE *fd = fopen(expt->userconfigfile, "rt");
  if(fd == NULL){
    strads_msg(ERR, "fata: mt-getuserconf: fails to open config file name %s \n", expt->userconfigfile);    
    exit(0);
  }  
  // YOUR JOB: set by default value for each parameters in expt data structure 
  /* by default */
  // Lasso example:
  expt->maxiter = 100;
  expt->lambda = 1.0;
  expt->timelimit = 100;
  expt->dfrequency=100;
  expt->outputdir=NULL;
  expt->scanwindow=100;
  expt->initminunit=0.1;  
  while(fgets(linebuf, IOHANDLER_BUFLINE_MAX, fd) != NULL){
    strads_msg(INF, "config file line : [%s] \n", linebuf);
    ptr = strtok(linebuf, USER_DELIMITERS);
    sscanf(ptr, "%s", param); 
    strads_msg(INF, " found param: %s\n", param);

    // YOUR JOB: copy the sample code and modify to handle your application specific entries. 
    // Lasso example: handle int type         
    if(param[0] == '#'){
      continue;
    }

    if(!strcmp(param, "#")){     
      continue;
    }

    if(!strcmp(param, "maxiter")){     
      ptr = strtok(NULL, USER_DELIMITERS);    
      strads_msg(INF, " found maxiter: %s\n", ptr);
      sscanf(ptr, "%d", &expt->maxiter);  
      strads_msg(INF, "\tmaxiter: %d\n", expt->maxiter);  
      continue;
    }
    
    if(!strcmp(param, "dfrequency")){     
      ptr = strtok(NULL, USER_DELIMITERS);
      strads_msg(INF, " found dfrequency: %s\n", ptr);
      sscanf(ptr, "%d", &expt->dfrequency);  
      strads_msg(INF, "\tdfrequency: %d\n", expt->dfrequency);  
      continue;
    }

    if(!strcmp(param, "timelimit")){     
      ptr = strtok(NULL, USER_DELIMITERS);
      strads_msg(INF, " found timelimit: %s\n", ptr);
      sscanf(ptr, "%lf", &expt->timelimit);  
      strads_msg(INF, "\ttimelimit: %lf\n", expt->timelimit);  
      continue;
    }

    if(!strcmp(param, "lambda")){     
      ptr = strtok(NULL, USER_DELIMITERS);
      strads_msg(INF, " lambda: %s\n", ptr);
      sscanf(ptr, "%lf", &expt->lambda);  
      strads_msg(INF, "\tlambda: %lf\n", expt->lambda);  
      continue;
    }


    if(!strcmp(param, "initminunit")){     
      ptr = strtok(NULL, USER_DELIMITERS);
      strads_msg(INF, " initminunit: %s\n", ptr);
      sscanf(ptr, "%lf", &expt->initminunit);  
      strads_msg(INF, "\tinitminunit: %lf\n", expt->initminunit);  
      continue;
    }


    if(!strcmp(param, "dpercentile")){     
      ptr = strtok(NULL, USER_DELIMITERS);
      strads_msg(INF, " dpercentile: %s\n", ptr);
      sscanf(ptr, "%lf", &expt->dpercentile);  
      strads_msg(INF, "\tdpercentile: %lf\n", expt->dpercentile);  
      continue;
    }

    if(!strcmp(param, "scanwindow")){     
      ptr = strtok(NULL, USER_DELIMITERS);
      strads_msg(INF, " scanwindow: %s\n", ptr);
      sscanf(ptr, "%ld", &expt->scanwindow);  
      strads_msg(INF, "\tscanwindow: %ld\n", expt->scanwindow);  
      continue;
    }
    
    if(!strcmp(param, "outputdir")){     
      ptr = strtok(NULL, USER_DELIMITERS);    
      strads_msg(INF, " found outputdir name: %s\n", ptr);
      expt->outputdir = (char *)calloc(strlen(ptr) + 10, sizeof(char));
      strcpy(expt->outputdir, ptr);
      strads_msg(INF, "\toutputdir name: %s\n", expt->outputdir);  
      continue;
    }

    strads_msg(ERR, "Warning: cmd[%s] unknown parameter. Modify getuserconf or config file \n", param); 
  }
  assert(expt->outputdir);
}

void user_printappcfg(experiments *expt){
  // YOUR JOB: add strads_msg showing your application specific configurations in expt data structure  
  // Lass Sample code: 
  strads_msg(ERR, "user_printappcfg: \n");
  strads_msg(ERR, "max iterations: %d \n", expt->maxiter);
  strads_msg(ERR, "dfrequency: %d \n", expt->dfrequency);
  strads_msg(ERR, "time limit: %lf seconds \n", expt->timelimit);

  strads_msg(ERR, "scan window: %ld \n", expt->scanwindow);
  strads_msg(ERR, "initial min unit: %lf \n", expt->initminunit);

  strads_msg(ERR, "output directory: %s \n", expt->outputdir);
  strads_msg(ERR, "@@ lambda: %2.20lf \n", expt->lambda);
  strads_msg(ERR, "@@ OOC Dpartitions %ld \n", expt->oocpartitions); 
  assert(expt->init_sweepoutrate >= 1.2);
  strads_msg(ERR, "@@ INIT SWEEP OUT RATE  %lf (should be at least 1.2)\n", expt->init_sweepoutrate); 

  return;
}


void _init_memalloc_userctx_app(problemctx *pctx, appschedctx *appctx){
  // OBSOLETE 
  return;
}

void _makedpartplan_by_sample_for_workers(threadctx *tctx, dpartitionctx *xdarray, uint64_t partcnt, uint64_t samples, uint64_t features){  
  uint64_t share  = samples / partcnt;
  uint64_t remain = samples % partcnt;
  uint64_t rprog=0;
  uint64_t myshare=0;
  uint64_t tmpend;
  clusterctx *cctx = tctx->cctx;
  strads_msg(ERR, "\t\t_makedpartplan_by_sample partitions(%ld)  samples(%ld) feature(%ld)\n", 
	     partcnt, samples, features);
  for(uint64_t i=0; i < partcnt; i++){
    if(i==0){
      //      xdarray[i].range.v_s = i*share;  
      xdarray[i].range.v_s = 0;  
    }else{      
      xdarray[i].range.v_s = xdarray[i-1].range.v_end +1;  
    }
    if(rprog++ < remain){
      myshare = share+1;
    }else{
      myshare = share;
    }
    tmpend = xdarray[i].range.v_s + myshare - 1;
    if(tmpend < samples){
      xdarray[i].range.v_end = tmpend;
    }else{
      xdarray[i].range.v_end = samples-1;
      if(i != partcnt-1){
	strads_msg(ERR, "____ fatal error in sample partitioning. \n");
	exit(-1);
      }      
    }
    xdarray[i].range.h_s = 0;
    xdarray[i].range.h_end = features-1;
    xdarray[i].range.v_len = xdarray[i].range.v_end - xdarray[i].range.v_s + 1;
    xdarray[i].range.h_len = xdarray[i].range.h_end - xdarray[i].range.h_s + 1;
    strads_msg(ERR, "\t\t i(%ld)  range.v_s(%ld)  v_end(%ld) v_len(%ld)\n", 
	       i, xdarray[i].range.v_s, xdarray[i].range.v_end, xdarray[i].range.v_len); 
  }
  strads_msg(ERR, "\t\tsend data partitioning plan to worker machines (%d) \n", cctx->wmachines);
}

void _send_plan_wait_for_loading(threadctx *tctx, dpartitionctx *xdarray, uint64_t machines, uint64_t samples, uint64_t features){

  problemctx *pctx = tctx->pctx;
  uint8_t *recvbuf=NULL;
  uint64_t nztotalsum=0;
  long ooc_hmodflag = xdarray->range.ooc_hmodflag;
  long h_modulo = xdarray->range.h_modulo;
  long oocdpartitions = xdarray->range.oocdpartitions;

  for(uint64_t i=0; i < machines; i++){
    send_ooc_dpartition_ooc_cmd_to_singlem(tctx, pctx->xfile, 
					   xdarray[i].range.v_s, xdarray[i].range.v_len, 
					   0,  features, 
					   colmajor_matrix, sparse_type, i, samples, features, 
					   ooc_hmodflag, h_modulo, oocdpartitions, load_dpart); 
    strads_msg(ERR, "\t\t\t\tMachine(%ld) v_s(%ld) v_end(%ld) v_len(%ld). h_s(%d) len(%ld) fullsam(%ld) fulldim(%ld) (flag:%ld, modulo:%ld, oodpartitions: %ld\n", 
	       i, xdarray[i].range.v_s, xdarray[i].range.v_end, xdarray[i].range.v_len, 0, features, samples, features, 
	       ooc_hmodflag, h_modulo, oocdpartitions);
  }

  for(unsigned int i=0; i < machines; i++){
    recvbuf = NULL;
    syscom_malloc_listento_port(tctx->comh[WGR], &recvbuf);
    if(recvbuf == NULL){
      strads_msg(ERR, "@@@ __ fatal with getting load confim\n");
    }

    com_header *headp = (com_header *)recvbuf;
    test_pkt *pkt = (test_pkt *)&recvbuf[sizeof(com_header)];
    strads_msg(ERR, "\t\t\t\t__ uploading confirm from (%d) machine a(%d)\n", headp->src_rank, pkt->a);
    nztotalsum += pkt->a;
    free(recvbuf);
  }

  strads_msg(ERR, "\n\n\n\n\n@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ All workers upload nz : %ld\n\n\n\n\n", nztotalsum);
}

void _send_oocplan_no_wait_for_loading(threadctx *tctx, dpartitionctx *xdarray, uint64_t machines, uint64_t samples, uint64_t features){
  problemctx *pctx = tctx->pctx;
  long ooc_hmodflag = xdarray->range.ooc_hmodflag;
  long h_modulo = xdarray->range.h_modulo;
  long oocdpartitions = xdarray->range.oocdpartitions;
  for(uint64_t i=0; i < machines; i++){
    send_ooc_dpartition_ooc_cmd_to_singlem(tctx, pctx->xfile, 
					   xdarray[i].range.v_s, xdarray[i].range.v_len, 
					   0,  features, 
					   colmajor_matrix, sparse_type, i, samples, features, 
					   ooc_hmodflag, h_modulo, oocdpartitions, oocload_dpart); 
    strads_msg(ERR, "\t\t\t\tMachine(%ld) v_s(%ld) v_end(%ld) v_len(%ld). h_s(%d) len(%ld) fullsam(%ld) fulldim(%ld) (flag:%ld, modulo:%ld, oodpartitions: %ld\n", 
	       i, xdarray[i].range.v_s, xdarray[i].range.v_end, xdarray[i].range.v_len, 0, features, samples, features, 
	       ooc_hmodflag, h_modulo, oocdpartitions);
  }
}

void _send_oocplan_no_wait_for_loading_for_schedulers(threadctx *tctx, dpartitionctx *xdarray, uint64_t machines, uint64_t samples, uint64_t features){
  problemctx *pctx = tctx->pctx;
  clusterctx *cctx = tctx->cctx;
  long ooc_hmodflag = xdarray->range.ooc_hmodflag;
  long h_modulo = xdarray->range.h_modulo;
  long oocdpartitions = xdarray->range.oocdpartitions;

  for(uint64_t i=0; i < machines; i++){
    send_ooc_dpartition_ooc_cmd_to_schedm(tctx, pctx->xfile, 
					  0,  samples, 
					  xdarray[i].range.h_s, xdarray[i].range.h_len, 
					  colmajor_matrix, sparse_type, i + cctx->wmachines, samples, features, 
					  ooc_hmodflag, h_modulo, oocdpartitions, oocload_dpart);
  }
}

void _makedpartplan_by_column_for_schedulers(threadctx *tctx, dpartitionctx *xdarray, uint64_t partcnt, uint64_t samples, uint64_t features){  
  uint64_t share = (features)/partcnt;
  uint64_t remain = features % partcnt;
  uint64_t tmpend, rprog=0, myshare=0;
  clusterctx *cctx = tctx->cctx;
  strads_msg(ERR, "\t\t_makedpartplan_by_column partitions(%ld)  samples(%ld) feature(%ld)\n", 
	     partcnt, samples, features);
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
    xdarray[i].range.v_s = 0;
    xdarray[i].range.v_end = samples-1;
    xdarray[i].range.v_len = xdarray[i].range.v_end - xdarray[i].range.v_s + 1;
    xdarray[i].range.h_len = xdarray[i].range.h_end - xdarray[i].range.h_s + 1;
    strads_msg(ERR, "\t\t @@@ Scheduler's plan i(%ld)  range.h_s(%ld)  h_end(%ld) h_len(%ld)\n", 
	       i, xdarray[i].range.h_s, xdarray[i].range.h_end, xdarray[i].range.h_len); 
  }
  strads_msg(ERR, "\t\tsend data partitioning plan to Scheduler machines (%d) \n", cctx->schedmachines);
}

void _send_plan_wait_for_loading_for_schedulers(threadctx *tctx, dpartitionctx *xdarray, uint64_t machines, uint64_t samples, uint64_t features){
  problemctx *pctx = tctx->pctx;
  uint8_t *recvbuf=NULL;
  clusterctx *cctx = tctx->cctx;
  uint64_t nztotalsum = 0;
  long ooc_hmodflag = xdarray->range.ooc_hmodflag;
  long h_modulo = xdarray->range.h_modulo;
  long oocdpartitions = xdarray->range.oocdpartitions;

  for(uint64_t i=0; i < machines; i++){
    strads_msg(ERR, "\t\t\t\tTo Machine(%ld) v_s(%ld) v_end(%ld) v_len(%ld). h_s(%ld) h_end(%ld) h_len(%ld) fullsam(%ld) fulldim(%ld)\n", 
	       i + cctx->wmachines, xdarray[i].range.v_s, xdarray[i].range.v_end, xdarray[i].range.v_len, 
	       xdarray[i].range.h_s, xdarray[i].range.h_end, xdarray[i].range.h_len, samples, features);
    send_ooc_dpartition_ooc_cmd_to_schedm(tctx, pctx->xfile, 
					  0,  samples, 
					  xdarray[i].range.h_s, xdarray[i].range.h_len, 
					  colmajor_matrix, sparse_type, i + cctx->wmachines, samples, features, 
					  ooc_hmodflag, h_modulo, oocdpartitions, load_dpart); 
  }

  for(unsigned int i=cctx->wmachines; i < cctx->wmachines + machines; i++){
    recvbuf = NULL;
    syscom_malloc_listento_port(tctx->comh[SGR], &recvbuf);
    if(recvbuf == NULL){
      strads_msg(ERR, "@@@ __ fatal with getting load confim\n");
    }
    com_header *headp = (com_header *)recvbuf;
    test_pkt *pkt = (test_pkt *)&recvbuf[sizeof(com_header)];
    strads_msg(ERR, "\t\t\t\t__ uploading confirm from (%d) machine a(%d)\n", headp->src_rank, pkt->a);
    nztotalsum += pkt->a;
    free(recvbuf);
  }

  strads_msg(ERR, "\n\n\n\n\n all schedulers upload nz : %ld\n", nztotalsum);

}

void _send_plan_wait_for_loadingY(threadctx *tctx, uint64_t machines, uint64_t samples, uint64_t features){

  problemctx *pctx = tctx->pctx;
  uint8_t *recvbuf=NULL;

  for(uint64_t i=0; i < machines; i++){
    send_dpartition_cmd_to_singlem(tctx, pctx->yfile, 
				   0, samples, 
				   0, features, 
				   colmajor_matrix, dense_type, i, samples, features); 
    strads_msg(ERR, "\t\t\t\tMachine(%ld) v_s(%d) v_end(%ld) v_len(%ld). h_s(%d) len(%ld) fullsam(%ld) fulldim(%ld)\n", 
	       i, 0, samples-1, samples, 0, features, samples, features);
  }

  for(unsigned int i=0; i < machines; i++){
    recvbuf = NULL;
    syscom_malloc_listento_port(tctx->comh[WGR], &recvbuf);
    if(recvbuf == NULL){
      strads_msg(ERR, "@@@ __ fatal with getting load confim\n");
    }
    com_header *headp = (com_header *)recvbuf;
    test_pkt *pkt = (test_pkt *)&recvbuf[sizeof(com_header)];
    strads_msg(ERR, "\t\t\t\t__ Y uploading confirm from (%d) machine a(%d)\n", headp->src_rank, pkt->a);
    free(recvbuf);
  }

}

void _print_sampleset(sampleset *set){
  strads_msg(ERR, "\nA round with (%ld) parameters\n", set->size);
  assert(set->size <= MAX_SAMPLE_SIZE);
  for(uint64_t i=0; i < set->size; i++){
    strads_msg(ERR, " %ld ", set->samples[i]);    
  }
  strads_msg(ERR, "\n\n");
}

/* soft_thrd, multiresidual  for workers */
double _soft_thrd(double sum, double lambda){
  double res;
  if(sum >=0){
    if(sum > lambda){
      res = sum - lambda;
    }else{
      res = 0;
    }
  }else{
    if(sum < -lambda){
      res = sum + lambda;
    }else{
      res = 0;
    }
  }
  return res;
}

void *worker_thread(void *arg){
  wbgctx *bgctx = (wbgctx *)arg;
  problemctx *pctx = bgctx->pctx;
  experiments *expt = bgctx->expt;
  appworker *awctx = bgctx->awctx;
  dpartitionctx *xdp = awctx->dpart[0];
  double *residual = awctx->residual;
  double *beta     = awctx->beta;
  double mpartialsum, xsqsum;
  double *yvector = awctx->yvector;
  strads_msg(INF, "\t[Worker(%d:%d) bootup] pctx(%p) expt(%p) awctx(%p)\n", 
	     bgctx->rank, bgctx->lid, pctx, expt, awctx);
  while(1){
    pthread_mutex_lock(&bgctx->mu);
    while(bgctx->queue.empty()){
      pthread_cond_wait(&bgctx->wakeupsignal, &bgctx->mu); 
    }   
    pthread_mutex_unlock(&bgctx->mu);    
    strads_msg(INF, "\t\tworker[%d][%d] thread got req\n", bgctx->rank, bgctx->lid);   
    iothread_item *task = bgctx->queue.front();  
    bgctx->queue.pop_front();   
    pthread_mutex_lock(&bgctx->mu);
    iothread_ret *retitem = new iothread_ret;
    retitem->size = 0;
    xdp = awctx->dpart[0];
    if(task->jobtype == j_update){
      for(uint64_t i=0; i < task->size; i++){
	if(task->vidlist[i] != BIGLASSO_INVALID_VID){	       
	  // calculate machine specific parital for given parameters 
	  strads_msg(INF, "Updater vid(%ld) beta = %lf\n", task->vidlist[i], beta[task->vidlist[i]]);
	  mpartialsum = _partialsum_update(xdp, task->vidlist[i], residual, beta[task->vidlist[i]], &xsqsum);
	  retitem->idmpartialpair[i].vid = task->vidlist[i];
	  retitem->idmpartialpair[i].mpartial = mpartialsum;	 
	  retitem->idmpartialpair[i].xsqmsum = xsqsum;	 	 
	}else{
	  // skip 
	  retitem->idmpartialpair[i].vid = task->vidlist[i];
	  retitem->idmpartialpair[i].mpartial = 0;	 
	  retitem->idmpartialpair[i].xsqmsum = 0;	 	 
	}     
	strads_msg(INF, "Updater Worker[%d:%d][%ld] mpartial(%lf) Resp(%p)\n", bgctx->rank, bgctx->lid,
		   retitem->idmpartialpair[i].vid, retitem->idmpartialpair[i].mpartial, residual);
	retitem->size++;
      } // for(uint64_t i.... 
    }// if(... jobtype == j_update ... 
    if(task->jobtype == j_residual){   
      uint64_t throw_s = awctx->thrange[bgctx->lid].v_s;
      uint64_t throw_end = awctx->thrange[bgctx->lid].v_end;
      uint64_t throw_len = awctx->thrange[bgctx->lid].v_len;      
      assert((throw_end - throw_s + 1) == throw_len);
      assert(throw_len != 0);
      idval_pair *betaidval = task->betaidval;
      uint64_t nbc = task->nbc;      
      for(uint64_t i=0; i<nbc; i++){	
	double newB = betaidval[i].beta;
	double oldB = betaidval[i].oldbeta;
	uint64_t vid = betaidval[i].vid;
	strads_msg(INF, "Residual worker(%d:%d) vid(%ld) newB(%lf) oldB(%lf) throw_s(%ld) throw_end(%ld) Resp(%p)\n", 
		   bgctx->rank, bgctx->lid, vid, newB, oldB, throw_s, throw_end, residual);  
	_update_residual_by_sample(xdp, newB, oldB, residual, vid, throw_s, throw_end);
      }
      retitem->size =0;
      retitem->jobtype = j_residual;
    }
    // TODO TODO TODO TODO 
    if(task->jobtype == j_objective){   
      uint64_t throw_s = awctx->thrange[bgctx->lid].v_s;
      uint64_t throw_end = awctx->thrange[bgctx->lid].v_end;
      uint64_t throw_len = awctx->thrange[bgctx->lid].v_len;      
      assert((throw_end - throw_s + 1) == throw_len);
      assert(throw_len != 0);
#if !defined(OBJ_USING_RESIDUAL)
      double thobjsum = _get_obj_partialsum(xdp, throw_s, throw_end, yvector, beta, xdp->fullfeatures);  
#else
      if(bgctx->lid == 0){
	strads_msg(INF, "I am using residual partial sum now \n");
      }
      double thobjsum = _get_obj_partialsum_with_residual(throw_s, throw_end, yvector, residual);  
#endif
      retitem->obthsum = thobjsum;
      retitem->size =0;
      retitem->jobtype = j_objective;
    }
    bgctx->retqueue.push_back(retitem);
    pthread_mutex_unlock(&bgctx->mu);    
    free(task);
  } // while(1)...
  return NULL;
}

// TODO Later, move objective function calculation to the scheduler 
double _get_obj_partialsum(dpartitionctx *dp, uint64_t row_s, uint64_t row_end, double *yv, double *beta, uint64_t features){
  double tmpxval, *rowsum, sum=0;
  rowsum = (double *)calloc((row_end - row_s + 1), sizeof(double));
  for(uint64_t col=0; col<features; col++){
    if(beta[col] == 0.0)
      continue;
    // TODO: later replace it with iterator... 
    for(uint64_t row=row_s; row <= row_end; row++){
      tmpxval = dp->spcol_mat.get(row, col);
      rowsum[row - row_s] += tmpxval*beta[col];
    }
  }
  for(uint64_t row=row_s; row <= row_end; row++){
    sum += ((yv[row] - rowsum[row - row_s])*(yv[row] - rowsum[row - row_s]));
  }
  sum = sum/2.0;
  free(rowsum);
  return sum;
}

// TODO Later, move objective function calculation to the scheduler 
double _get_obj_partialsum_with_residual(uint64_t row_s, uint64_t row_end, double *yv,  double *residual){
  double sum=0;
  for(uint64_t row=row_s; row <= row_end; row++){
    //    sum += ((yv[row] - residual[row])*(yv[row] - residual[row]));
    sum += (residual[row]*residual[row]);
  }
  sum = sum/2.0;
  return sum;
}

double _partialsum_update(dpartitionctx *dp, uint64_t vid, double *residual, double vidbeta, double *xsqmsum){
  double thsum = 0;
  uint64_t tmprow;
  double xsq=0;
  assert(vid != BIGLASSO_INVALID_VID);
  for( auto p : dp->spcol_mat.col(vid)){
    tmprow = p.first;
    if(tmprow < dp->range.v_s){
      strads_msg(ERR, " out of range in partial sum update \n");
      exit(0);
    }
    if(tmprow > dp->range.v_end){
      strads_msg(ERR, " out of range in partial sum update \n");
      exit(0);
    }
    thsum += ((vidbeta*p.second*p.second ) + p.second*residual[tmprow]);
    //thsum += p.second*residual[tmprow];
    xsq += (p.second * p.second);
  }     
  *xsqmsum = xsq;
  return thsum;
}

void _update_residual_by_sample(dpartitionctx *dp, double newB, double oldB, double *residual, uint64_t vid, uint64_t row_s, uint64_t row_end){
  assert(vid != BIGLASSO_INVALID_VID);
  double delta=0, tmpval=0, oldRi;
  //  oldB = oldbeta[vid];
  //newB = beta[vid];    
  for(uint64_t row=row_s; row <= row_end; row++){
    tmpval = dp->spcol_mat.get(row, vid);
    if(tmpval == 0.0)
      continue;
    if(newB != 0.0){
      delta = tmpval*(oldB - newB); 
    }else{
      // newb == 0, oldb != 0 
      if(oldB != 0.0){
	delta = tmpval*(oldB);
      }
    }
    oldRi = residual[row];    
    residual[row] = residual[row] + delta;
    strads_msg(INF, "residual update for vid(%ld) row(%ld) oldRes (%lf)  --> newRes (%lf)    xval(%lf)  delta(%lf)\n", 
	       vid, row, oldRi, residual[row], tmpval, delta);
  }
  return;
}

uint64_t _nzcount(double *beta, uint64_t length){
 uint64_t ret = 0;
  for(uint64_t i=0; i < length; i++){
    if(beta[i] != 0.0)
      ret++;
  }
  return ret;
}
