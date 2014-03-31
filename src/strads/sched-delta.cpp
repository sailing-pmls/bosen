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
#include <assert.h>
#include <cstdlib>      // std::rand, std::srand
#include <list>
#include <pthread.h>
#include <map>
#include <time.h>
#include <math.h>
#include "threadctx.hpp"
#include "basectx.hpp"
#include "utility.hpp"
#include "strads-queue.hpp"
#include "getcorr.hpp"
#include "sched-delta.hpp"
#include "systemmacro.hpp"
#include "syscom.hpp"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

using namespace std;

uint64_t _get_partno(uint64_t gvid, threadctx *tctx);

void psched_process_delta_update_with_partno(deltaset *deltas, threadctx *tctx, double newminunit, uint64_t newdesired, int partno, uint64_t blockcmd){

  uint64_t *idxlist = deltas->idxlist;
  double *deltalist = deltas->deltalist;
  long cnt = deltas->size;

  
  clusterctx *cctx = tctx->cctx;
  uint64_t machid = (partno/cctx->schedthrd_per_mach); // local scheduler machine id 
  uint64_t dstmachine = machid + tctx->cctx->wmachines; // absolute machine id 

  if(cnt > MAX_DELTA_SIZE){
    strads_msg(ERR, "Fatal: increase MAX_DELTA_SIZE\n");
    exit(0);
  }

  strads_msg(INF, "\n ++ Process Delta Update: cnt: %ld\n", cnt);
  delta_entry *dpentry = (delta_entry *)calloc(cnt+10, sizeof(delta_entry));

  if(dpentry == NULL){
    strads_msg(ERR, "Error in memory allocation in process delta update \n");
    exit(0);
  }

  for(int i=0; i < cnt; i++){
    // TODO remove this assert ..... 
    //    assert(partno == _get_partno(idxlist[i], tctx));

    dpentry[i].idx = idxlist[i];
    dpentry[i].delta = deltalist[i];
  }

  strads_msg(INF, "\t\t @ sendamocomhandler: size: %ld partno(%d)\n", 
	     sizeof(delta_entry)*cnt, partno);
	
  dpentry[cnt].idx = DESIRED_CHANGE_SIGNATURE;
  dpentry[cnt].delta = (double)newdesired;
  cnt++;

  dpentry[cnt].idx = MIN_UNIT_CHANGE_SIGNATURE;
  dpentry[cnt].delta = newminunit;
  cnt++;

  //  syscom_send_amo_comhandler(tctx->comh[SGR], tctx, (uint8_t *)dpentry, sizeof(delta_entry)*cnt, 
  //			     deltaupdate, dstmachine, partno);      
  syscom_send_amo_comhandler_with_blockcmd(tctx->comh[SGR], tctx, (uint8_t *)dpentry, sizeof(delta_entry)*cnt, 
					    deltaupdate, dstmachine, partno, blockcmd);      

  free(dpentry);
}





void psched_process_delta_update_onepartition(deltaset *deltas, threadctx *tctx, double newminunit, uint64_t newdesired, uint64_t onepart, uint64_t blockcmd){

  uint64_t *idxlist = deltas->idxlist;
  double *deltalist = deltas->deltalist;
  long cnt = deltas->size;

  if(cnt > MAX_DELTA_SIZE){
    strads_msg(ERR, "Fatal: increase MAX_DELTA_SIZE\n");
    exit(0);
  }

  strads_msg(INF, "\n ++ Process Delta Update: cnt: %ld\n", cnt);
  // partition number corresponds to the of scheduler threads 
  int pcnt = (tctx->cctx->schedmachines-1)*tctx->cctx->schedthrd_per_mach;
  delta_entry **dpart = (delta_entry **)calloc(pcnt, sizeof(delta_entry *));

  if(dpart == NULL){
    strads_msg(ERR, "Error in memory allocation in process delta update \n");
    exit(0);
  }
  assert(dpart);
  int *carray = (int *)calloc(pcnt, sizeof(int));
  assert(carray);
  clusterctx *cctx = tctx->cctx;
  long unsigned int partno=0;
  long unsigned int maxfeature_per_thread = (tctx->pctx->features/pcnt)+1;

  for(int i=0; i < pcnt; i++){
    dpart[i] = (delta_entry *)calloc(cnt+10, sizeof(delta_entry));
    assert(dpart[i]);
  }

  for(int i=0; i < cnt; i++){
    //    partno = idxlist[i]/maxfeature_per_thread;
    partno = _get_partno(idxlist[i], tctx);

    assert(partno == onepart);

    dpart[partno][carray[partno]].idx = idxlist[i];
    dpart[partno][carray[partno]].delta = deltalist[i];
    carray[partno]++;
  }

  int localcnt=0;
  for(int i=tctx->cctx->wmachines; i<tctx->cctx->totalmachines-1 ; i++){
    for(int j=0; j<cctx->schedthrd_per_mach; j++){

      partno = cctx->schedthrd_per_mach*localcnt + j;

      //strads_msg(ERR, "APP SCHEDULER @@@@@ Partno [%d]\n", partno);
      if(carray[partno] > 0){
#if 0      
	strads_msg(INF, "[App-Sched] rank(%d) sends delta update packet to %d pktsize[%ld] \n", 
		   tctx->rank, i, sizeof(delta_entry)*carray[partno]);      	
	for(int k=0; k < carray[partno]; k++){
	  strads_msg(INF, "\t\t\t\t @@@@@ [%d]th %ld partno -- idx(%ld) \n", 
		     k, partno, dpart[partno][k].idx);
	}
#endif 
	strads_msg(INF, "\t\t @ sendamocomhandler: size: %ld i(%d) partno(%ld)  dpart[partno]=(%p) with BLOCK CMD (%ld)\n", 
		   sizeof(delta_entry)*carray[partno], i, partno, (uint8_t *)dpart[partno], blockcmd );
	
	/*
       	syscom_send_amo_comhandler(tctx->comh[SGR], tctx, (uint8_t *)dpart[partno], sizeof(delta_entry)*carray[partno], 
				   deltaupdate, i, partno);      
	*/
	dpart[partno][carray[partno]].idx = DESIRED_CHANGE_SIGNATURE;
	dpart[partno][carray[partno]].delta = (double)newdesired;
	carray[partno]++;

	dpart[partno][carray[partno]].idx = MIN_UNIT_CHANGE_SIGNATURE;
	dpart[partno][carray[partno]].delta = newminunit;
	carray[partno]++;

	syscom_send_amo_comhandler_with_blockcmd(tctx->comh[SGR], tctx, (uint8_t *)dpart[partno], sizeof(delta_entry)*carray[partno], 
				   deltaupdate, i, partno, blockcmd);      

      }
    }
    localcnt++;
  }
  for(int i=0; i < pcnt; i++){
    free(dpart[i]);
  }
  free(dpart);
  free(carray);
}


































int psched_check_interference(st_sample *stsample, st_priority *stpriority, long desired, long result, sghthreadctx *tctx){

  assert(stsample->rsamplesize == result);
  assert(result > 1);
#if defined(DENSE_INPUT)
  problemctx *pctx=tctx->pctx;
  double **cmx=pctx->cmX;
  uint64_t samples=pctx->samples;
#endif 
  uint64_t gvidsample=0, lvidsample=0; 
  long spos=0;
  long gvidret=0, lvidret=0, chkpos=0, rvidxsize=0;
  uint64_t *rvidx=(uint64_t *)calloc(result, sizeof(uint64_t));
  double infdegree=0;
  bool conflict=false;

  dpartitionctx *dp = tctx->xdp;

#if defined(DENSE_INPUT)
  assert(tctx->pctx);
  assert(tctx->pctx->cmX);
#endif 

  assert(tctx->xdpready);
  assert(dp);
  assert(rvidx);
  // local vid + tctx->start_feature 
  rvidx[0] = stsample->sample[0];
  rvidxsize=1;

  for(spos=1; spos < result; spos++){

    lvidsample = stsample->sample[spos];  
    gvidsample = lvidsample + tctx->start_feature;


    if(gvidsample > tctx->end_feature){
      strads_msg(ERR, "Fatal lvidsample (%ld) gid(%d)'s start_feature: %ld end: %ld\n",
		 lvidsample, tctx->gid, tctx->start_feature, tctx->end_feature);
    }


    conflict=false;
    // see if this lvidsample does cause conflict with existing ones in rvidx list.  
    for(chkpos = 0; chkpos < rvidxsize; chkpos++){

      assert(rvidxsize < result);

      lvidret = rvidx[chkpos];
      gvidret= lvidret + tctx->start_feature;

    if(gvidret > tctx->end_feature){
      strads_msg(ERR, "Fatal lvidret (%ld) gid(%d)'s start_feature: %ld end: %ld\n",
		 lvidret, tctx->gid, tctx->start_feature, tctx->end_feature);
    }

#if defined(DENSE_INPUT)      
      infdegree = getcorr_pair(cmx[gvidsample], cmx[gvidret], samples, 
			       pctx->pxsum[gvidsample], pctx->pxsqsum[gvidsample], 
			       pctx->pxsum[gvidret], pctx->pxsqsum[gvidret], 1);
#endif 



      strads_msg(INF, "####### tctx->rank(%d)(%d)(%d) gvsam(%ld)loc%ld(%ld)  gvret(%ld)loc%ld(%ld) \n", 
		 tctx->rank, tctx->lid, tctx->gid,  gvidsample, lvidsample, dp->spcol_mat.col(gvidsample).size(), 
		 gvidret, lvidret, dp->spcol_mat.col(gvidret).size());


      assert(gvidsample < tctx->pctx->features);
      assert(gvidret < tctx->pctx->features);



      if(dp->spcol_mat.col(gvidsample).size() == 0 || dp->spcol_mat.col(gvidret).size() == 0){
	strads_msg(ERR, "X.col(%ld).size(%ld) , X.col(%ld).size(%ld) abnormal\n", 
		   gvidsample, dp->spcol_mat.col(gvidsample).size(), 
		   gvidret,   dp->spcol_mat.col(gvidret).size());
      }

      infdegree = getcorr_pair_sparsem(dp->spcol_mat.col(gvidsample), dp->spcol_mat.col(gvidret), dp->fullsamples, 
				       0, 0, 0, 0, 0);
      
      if(infdegree >= PSCHED_INF_THRESHOLD){
	strads_msg(INF, "@@ 2@@ CORR between (%ld)(%ld): %lf is too high \n", lvidsample, lvidret, infdegree);
	conflict = true;
	break;
      }
    }

    if(!conflict){ // if false - does not cause conflicts
      rvidx[rvidxsize] = lvidsample;
      rvidxsize++;
    }
    if(rvidxsize >= desired || rvidxsize >= result){
      break;
    }		   
  }
  for(long i=0; i < rvidxsize; i++){
    stsample->sample[i] = rvidx[i];
  }
  stsample->rsamplesize = rvidxsize;

  free(rvidx);


  strads_msg(INF, "full interference checking target(%ld) survive %ld \n", result, rvidxsize); 
  return rvidxsize;
}

int psched_light_check_interference(st_sample *stsample, st_priority *stpriority, long desired, long result, sghthreadctx *tctx){

  assert(stsample->rsamplesize == result);
  assert(result > 1);

  uint64_t carecnt = LIGHT_CARE_CNT;
  if(carecnt > result){
    carecnt = result/2;
    assert(carecnt > 1);
  }

  //  uint64_t carevid[20];
  //  uint64_t workvid[20];
  uint64_t *carevid = (uint64_t *)calloc(carecnt*2, sizeof(uint64_t));
  uint64_t *workvid = (uint64_t *)calloc(carecnt*2, sizeof(uint64_t));
  assert(carevid);
  assert(workvid);

  uint64_t spos=0, chkpos=0, rvidxsize=0, newlvid, newgvid, oldlvid, oldgvid;
  uint64_t *rvidx=(uint64_t *)calloc(result, sizeof(uint64_t));
  assert(rvidx);

  double infdegree=0;
  bool conflict=false;
  dpartitionctx *dp = tctx->xdp;

  dataset_t tvidmap;
  value_index_t& tvidmap_index = tvidmap.get<value_tag>();

  assert(tctx->xdpready);
  assert(dp);
  assert(rvidx);
  
  for(uint64_t i=0; i < (uint64_t)result; i++){
    uint64_t  lvid = stsample->sample[i];
    double    delta = stpriority->pool[lvid];
    tvidmap.insert(element_t(lvid, delta));		 
  }

  value_index_t::iterator maxit = tvidmap_index.begin();
  uint64_t ntcnt=0;
  for(; maxit != tvidmap_index.end(); maxit++){
    rvidx[ntcnt++] = maxit->first;    
    if(ntcnt == result - carecnt){
      maxit++;
      break;    
    }
  }// passing the unnecessary ones since tividex has asecending order. 
  // todo: switch to rbegin()...

  uint64_t ccnt=0;
  for(; maxit != tvidmap_index.end(); maxit++){
    carevid[ccnt] = maxit->first;    
    ccnt++;
  }
  assert(ccnt == carecnt);

  workvid[0] = carevid[0];
  ccnt = 1;
  
  for(spos=1; spos < carecnt; spos++){
    newlvid = carevid[spos];
    newgvid = newlvid + tctx->start_feature;        
    conflict = false;

    // compare new one with existing ones 
    for(chkpos=0; chkpos<ccnt; chkpos++){ // existing ones... 
      oldlvid = workvid[chkpos];
      oldgvid = oldlvid + tctx->start_feature;


      if(dp->spcol_mat.col(newgvid).size() == 0 || dp->spcol_mat.col(oldgvid).size() == 0){
	strads_msg(ERR, "X.col(%ld).size(%ld) , X.col(%ld).size(%ld) abnormal\n", 
		   newgvid, dp->spcol_mat.col(newgvid).size(), 
		   oldgvid,   dp->spcol_mat.col(oldgvid).size());
      }

      infdegree = getcorr_pair_sparsem(dp->spcol_mat.col(newgvid), dp->spcol_mat.col(oldgvid), dp->fullsamples, 
				       0, 0, 0, 0, 0);
      if(infdegree >= PSCHED_INF_THRESHOLD){
	strads_msg(INF, "CORR between (%ld)(%ld): %lf is too high \n", newgvid, oldgvid, infdegree);	
		conflict = true;
	break;
      }
    }

    if(!conflict){ // if false - does not cause conflicts
      workvid[ccnt] = newlvid;
      ccnt++;
    }
    if(ccnt >= carecnt){
      break;
    }		   
  }
			     

  for(uint64_t i=0; i < ccnt ; i++){
    rvidx[ntcnt + i] = workvid[i];
  }

  rvidxsize = ntcnt + ccnt;   
  for(uint64_t i=0; i < rvidxsize; i++){
    stsample->sample[i] = rvidx[i];
  }

  stsample->rsamplesize = rvidxsize;
  free(rvidx);

  free(carevid);
  free(workvid);

  strads_msg(INF, "Partial top 10 vars interference checking target(%ld) survive %ld \n", result, rvidxsize); 
  return rvidxsize;
}


/* stsample data structure for return results 
   stdelta: data structure for delta of each task and sampling 
*/
int psched_init_sampling(st_priority *stpriority, long maxpoolsize, st_sample *stsample, long maxsetsize, st_prioritychange *pchange, long maxsize){

  double fullrangesize = 0;
  /* prevent dangling ptrs */
  assert(stpriority->pool == NULL);
  assert(stsample->sample == NULL);
  assert(stsample->rndpoints == NULL);
  assert(pchange->taskid == NULL);
  assert(pchange->newpriority == NULL);
  stpriority->priority_minunit = 1.0;
  // stpriority
  stpriority->size = maxpoolsize;
  stpriority->maxpoolsize = maxpoolsize;
  stpriority->pool = (double *)calloc(maxpoolsize, sizeof(double));
  assert(stpriority->pool);
  for(long i=0; i<stpriority->size; i++){
    //    stpriority->pool[i] = PRIORITY_MINUNIT;
    fullrangesize += stpriority->pool[i];
  }

  //  stpriority->fullrangesize = fullrangesize;
  stpriority->fullrangesize = 0;

  // stsample 
  stsample->maxsamplesize = maxsetsize;
  stsample->sample = (long *)calloc(maxsetsize, sizeof(long));  
  stsample->rndpoints = (double *)calloc(maxsetsize, sizeof(double));
  assert(stsample->sample);  

  // st_prioritychange
  pchange->maxsize = maxsetsize;
  pchange->taskid = (long *)calloc(maxsetsize, sizeof(long)); // local offset id   
  pchange->newpriority = (double *)calloc(maxsetsize, sizeof(double));  

  strads_msg(INF, "max   pool size  %ld \n", stpriority->maxpoolsize);
  strads_msg(INF, "max sample size: %ld \n", stsample->maxsamplesize); 
  strads_msg(INF, "max prioritychange size: %ld \n", pchange->maxsize); 
  strads_msg(INF, "stpriority->fullrange: %20.20lf \n", stpriority->fullrangesize); 
  
#if defined(FIXED_ORDER_ONE_SCHEDULER)
  srand(0x1020);
#endif

  return 0;
}

void _sort_rndpoints(double *rndlist, long size){
  dataset_t sortmap;
  value_index_t &sortmap_valueidx = sortmap.get<value_tag>();
  long idx=0;
  for(long i=0; i < size; i++){
    sortmap.insert(element_t(i, fabs(rndlist[i])));
  }
  for(value_index_t::iterator it = sortmap_valueidx.begin(); it != sortmap_valueidx.end(); it++){
    rndlist[idx] = it->second;
    idx++;
  }
  sortmap.clear();

}

int psched_do_sampling(st_sample *stsample, st_priority *stpriority, long desired){

  double *pool = stpriority->pool;

  double fullrangesize = stpriority->fullrangesize + stpriority->priority_minunit*stpriority->size;

  double rndpts, acc;
  long poolsize = stpriority->size;
  long rndptsidx, sampleidx, dupcnt;
  long *tmpsamples = stsample->sample;
  long cancelled=0;

  // sanity checking 
  assert(desired < stpriority->size);
  assert(desired < stsample->maxsamplesize);
  assert(desired < stpriority->maxpoolsize);
#if 0
  double rangesum=0;
  for(long i=0; i < stpriority->size; i++){
    rangesum += stpriority->pool[i];
  }
  stpriority->fullrangesize =  rangesum;
  strads_msg(INF, "\t\t\t ++psched_do_sampling desired %ld poolsize: %ld, fullrangesize %lf", 
	     desired, poolsize, fullrangesize);
#endif

  /* Step 1:  pick up random n points from 0 to fullrangesize   */
  for(long i=0; i < desired; i++){
    rndpts = utility_get_double_random(0, fullrangesize);
    stsample->rndpoints[i] = rndpts;
  }
  _sort_rndpoints(stsample->rndpoints, desired);
  /* Step 2: searching for index corresponds to the n random points */
  acc =0;
  rndptsidx=0;
  sampleidx=0;
  for(long i=0; i < poolsize; i++){ // move forward on poolindex
    if((acc + pool[i] + (i + 1)*stpriority->priority_minunit) >= stsample->rndpoints[rndptsidx]){     
      assert(sampleidx < stsample->maxsamplesize);
      tmpsamples[sampleidx] = i;
      strads_msg(INF, "%ldth task is chosen as %ldth sample by %ldth rndpt %lf \n", 
		 i, sampleidx, rndptsidx, stsample->rndpoints[rndptsidx]);
      sampleidx++; // move forward on sample idx
      dupcnt=0;
      while(acc + pool[i] + (i + 1)*stpriority->priority_minunit >= stsample->rndpoints[rndptsidx]){
	/* if more than one random points fall into a task, assign only one chance */
	dupcnt++;	
	rndptsidx++; // move forward on rndptsidx 
	if(rndptsidx >= desired)
	  break;	
      }      
      if(dupcnt > 1){
	strads_msg(INF, "  more than one random pts fall into the [%ld]th task's range, move %ld slots\n", i, dupcnt);
      } 
      if(rndptsidx >= desired){
	break;
      }
      cancelled += (dupcnt -1);
    }
    acc += pool[i];
  }

  for(long i=0; i<sampleidx; i++){
    stsample->sample[i] = tmpsamples[i];
  }
  stsample->rsamplesize = sampleidx;
  strads_msg(INF, "Desired %ld but cancelled %ld due to duplication\n",
	     desired, cancelled);
  return sampleidx;
}


int psched_update_priority(st_priority *stpriority, st_prioritychange *pchange){

  long changedid;
  double oldpriority, diff, newpriority;

  strads_msg(INF, "\t\t\t\t ++ update_priority: change size %ld \n", pchange->changed);
  assert(pchange->changed <= pchange->maxsize);
  assert(pchange->changed <= stpriority->size);
  assert(pchange->changed <= stpriority->maxpoolsize);
 
  double fullrangeprev;

  for(long i=0; i < pchange->changed; i++){

    changedid = pchange->taskid[i];
    assert(changedid < stpriority->maxpoolsize);
    assert(changedid < stpriority->size);   

    newpriority = fabs(pchange->newpriority[i]);
    //    if(newpriority < PRIORITY_MINUNIT)
    //  newpriority = PRIORITY_MINUNIT;

    //newpriority =  newpriority + PRIORITY_MINUNIT;
    //    newpriority =  newpriority;

    //newpriority = roundl(newpriority*10000000.0)/10000000.0;
    //assert(newpriority >= PRIORITY_MINUNIT);

    oldpriority = fabs(stpriority->pool[changedid]);
    diff = newpriority - oldpriority;

    stpriority->pool[changedid] = newpriority;       

    fullrangeprev = stpriority->fullrangesize;
    stpriority->fullrangesize = stpriority->fullrangesize + diff;

    if(stpriority->fullrangesize < 0.0){

      strads_msg(ERR, " fullrangesize : %2.20lf partno (%d)  diff(%2.20lf) local id(%ld)  entry(%2.20lf) <---(%2.20lf)\n", 
		 stpriority->fullrangesize, stpriority->partno, diff, changedid, stpriority->pool[changedid], oldpriority);
      //strads_msg(ERR, "cancel updating detla (%2.20lf) reset to previous (%2.20lf). poolentry(%2.20lf)-->(%2.20lf) \n", 
      //		 stpriority->fullrangesize, fullrangeprev, stpriority->pool[changedid], oldpriority);

      //      stpriority->fullrangesize = fullrangeprev;
      //stpriority->pool[changedid] = oldpriority;

    }
    assert(stpriority->fullrangesize >= 0.0);

  }
  // verification of fullrnage size and aggregate of priorities in the pool


#if 1
  double rangesum=0;
  for(long i=0; i < stpriority->size; i++){
    rangesum += stpriority->pool[i];
  }

  stpriority->fullrangesize = rangesum;

#endif

#if 0
  double rangesum=0;
  for(long i=0; i < stpriority->size; i++){
    rangesum += stpriority->pool[i];
  }
  //  if(rangesum != stpriority->fullrangesize){
  if(fabs(rangesum - stpriority->fullrangesize) > 0.1){    
    strads_msg(ERR, "[*******ERROR] stpriority->fullrange(%20.20lf) != sum(%20.20lf)  %lf\n", 
	       stpriority->fullrangesize, rangesum, stpriority->fullrangesize);
    assert(0);
  }else{
    strads_msg(ERR, "[*******GOOD] stpriority->fullrange(%lf) == sum(%lf)\n", 
	       stpriority->fullrangesize, rangesum);
  }
#endif

  return 0;
}

uint64_t psched_get_samples(void *recvbuf, sampleset *set){
  uint8_t *tmp = (uint8_t *)recvbuf;
  com_header *headp = (com_header *)recvbuf;
  uint64_t partno = headp->partition;

  set->size = headp->length/(sizeof(uint64_t));
  assert(set->size > 0);
  assert(set->size <= MAX_SAMPLE_SIZE);
  uint64_t * dset = (uint64_t *)&tmp[sizeof(com_header)];	  
  for(uint64_t i=0; i < set->size; i++){
    set->samples[i] = dset[i];
  }

  return partno;
}

















uint64_t _get_partno(uint64_t gvid, threadctx *tctx){

  problemctx *pctx = tctx->pctx;
  clusterctx *cctx = tctx->cctx;
  uint64_t maxshare = (pctx->features/pctx->prangecnt) + 1;
  uint64_t startpos = gvid / maxshare;
  uint64_t ret;
  bool found = false;

  for(int i=startpos; startpos < pctx->prangecnt; i++){
    uint64_t thrd_startc=pctx->prange[i].feature_start;
    uint64_t thrd_endc=pctx->prange[i].feature_end;
    if( (gvid >= thrd_startc) && (gvid <= thrd_endc)){
      ret = i;
      found = true;
      break;
    }
  }

  if(!found){
    strads_msg(ERR, "__ get_partno calculation gvid(%ld) can not find right range \n", gvid);
    assert(0);
    exit(0);
  }

  return ret;
}


// take a list of changed delta 
// deliver them to the right scheduler machines in charge of the chunk that 
// the associated parameter belongs to
void psched_process_delta_update(deltaset *deltas, threadctx *tctx, double newminunit, uint64_t newdesired){

  uint64_t *idxlist = deltas->idxlist;
  double *deltalist = deltas->deltalist;
  long cnt = deltas->size;

  if(cnt > MAX_DELTA_SIZE){
    strads_msg(ERR, "Fatal: increase MAX_DELTA_SIZE\n");
    exit(0);
  }

  strads_msg(INF, "\n ++ Process Delta Update: cnt: %ld\n", cnt);
  // partition number corresponds to the of scheduler threads 
  int pcnt = (tctx->cctx->schedmachines-1)*tctx->cctx->schedthrd_per_mach;
  delta_entry **dpart = (delta_entry **)calloc(pcnt, sizeof(delta_entry *));

  if(dpart == NULL){
    strads_msg(ERR, "Error in memory allocation in process delta update \n");
    exit(0);
  }
  assert(dpart);
  int *carray = (int *)calloc(pcnt, sizeof(int));
  assert(carray);
  clusterctx *cctx = tctx->cctx;
  long unsigned int partno=0;
  long unsigned int maxfeature_per_thread = (tctx->pctx->features/pcnt)+1;

  for(int i=0; i < pcnt; i++){
    dpart[i] = (delta_entry *)calloc(cnt+10, sizeof(delta_entry));
    assert(dpart[i]);
  }

  for(int i=0; i < cnt; i++){
    //    partno = idxlist[i]/maxfeature_per_thread;
    partno = _get_partno(idxlist[i], tctx);

    dpart[partno][carray[partno]].idx = idxlist[i];
    dpart[partno][carray[partno]].delta = deltalist[i];
    carray[partno]++;
  }

  int localcnt=0;
  for(int i=tctx->cctx->wmachines; i<tctx->cctx->totalmachines-1 ; i++){
    for(int j=0; j<cctx->schedthrd_per_mach; j++){

      partno = cctx->schedthrd_per_mach*localcnt + j;

      //strads_msg(ERR, "APP SCHEDULER @@@@@ Partno [%d]\n", partno);
      if(carray[partno] > 0){
#if 0      
	strads_msg(INF, "[App-Sched] rank(%d) sends delta update packet to %d pktsize[%ld] \n", 
		   tctx->rank, i, sizeof(delta_entry)*carray[partno]);      	
	for(int k=0; k < carray[partno]; k++){
	  strads_msg(INF, "\t\t\t\t @@@@@ [%d]th %ld partno -- idx(%ld) \n", 
		     k, partno, dpart[partno][k].idx);
	}
#endif 
	strads_msg(INF, "\t\t @ sendamocomhandler: size: %ld i(%d) partno(%ld)  dpart[partno]=(%p)\n", 
		   sizeof(delta_entry)*carray[partno], i, partno, (uint8_t *)dpart[partno]);
	
	/*
       	syscom_send_amo_comhandler(tctx->comh[SGR], tctx, (uint8_t *)dpart[partno], sizeof(delta_entry)*carray[partno], 
				   deltaupdate, i, partno);      
	*/
	dpart[partno][carray[partno]].idx = DESIRED_CHANGE_SIGNATURE;
	dpart[partno][carray[partno]].delta = (double)newdesired;
	carray[partno]++;


	dpart[partno][carray[partno]].idx = MIN_UNIT_CHANGE_SIGNATURE;
	dpart[partno][carray[partno]].delta = newminunit;
	carray[partno]++;

	syscom_send_amo_comhandler(tctx->comh[SGR], tctx, (uint8_t *)dpart[partno], sizeof(delta_entry)*carray[partno], 
				   deltaupdate, i, partno);      
      }
    }
    localcnt++;
  }
  for(int i=0; i < pcnt; i++){
    free(dpart[i]);
  }
  free(dpart);
  free(carray);
}



// you set deltas to NULL, it will start uniform scheduling with uniform deltas 
void psched_start_scheduling(deltaset *deltas, threadctx *tctx, double newminunit){

  if(deltas != NULL){
    psched_process_delta_update(deltas, tctx, newminunit, 0);
  }else{

    clusterctx *cctx = tctx->cctx;
    deltaset *deltas = (deltaset *)calloc(1, sizeof(deltaset));
    uint64_t partitions = (cctx->schedthrd_per_mach*(cctx->schedmachines-1));
    uint64_t spos = (tctx->pctx->features/partitions)+1;

    for(uint64_t i=0; i < partitions; i++){

      //      deltas->idxlist[i] = i*spos;
      deltas->idxlist[i] = tctx->pctx->prange[i].feature_start;

      deltas->deltalist[i] = 0.0;
    }
    deltas->size = partitions; 
    psched_process_delta_update(deltas, tctx, newminunit, 0);
    free(deltas);
  }
}


void psched_send_token(threadctx *tctx, uint64_t partno){

  clusterctx *cctx = tctx->cctx;
  uint64_t machid = (partno/cctx->schedthrd_per_mach); // local scheduler machine id 
  uint64_t dstmachine = machid + tctx->cctx->wmachines; // absolute machine id 

  syscom_send_empty_amo_comhandler(tctx->comh[SGR], tctx, NULL, 0, deltaupdate, dstmachine, partno);	
}


void psched_send_blockcmd(threadctx *tctx, uint64_t partno, ptype packettype, uint64_t blockcmd){ // wake up 

  clusterctx *cctx = tctx->cctx;
  uint64_t machid = (partno/cctx->schedthrd_per_mach); // local scheduler machine id 
  uint64_t dstmachine = machid + tctx->cctx->wmachines; // absolute machine id 

  syscom_send_empty_amo_comhandler_with_blockcmd(tctx->comh[SGR], tctx, NULL, 0, packettype, dstmachine, partno, blockcmd);
}




#if 0 
// take a list of changed delta 
// deliver them to the right scheduler machines in charge of the chunk that 
// the associated parameter belongs to
void psched_process_delta_update(deltaset *deltas, threadctx *tctx, double newminunit){
  uint64_t *idxlist = deltas->idxlist;
  double *deltalist = deltas->deltalist;
  long cnt = deltas->size;
  if(cnt > MAX_DELTA_SIZE){
    strads_msg(ERR, "Fatal: increase MAX_DELTA_SIZE\n");
    exit(0);
  }
  strads_msg(INF, "\n ++ Process Delta Update: cnt: %ld\n", cnt);
  // partition number corresponds to the of scheduler threads 
  int pcnt = (tctx->cctx->schedmachines-1)*tctx->cctx->schedthrd_per_mach;
  delta_entry **dpart = (delta_entry **)calloc(pcnt, sizeof(delta_entry *));
  if(dpart == NULL){
    strads_msg(ERR, "Error in memory allocation in process delta update \n");
    exit(0);
  }
  assert(dpart);
  int *carray = (int *)calloc(pcnt, sizeof(int));
  assert(carray);
  clusterctx *cctx = tctx->cctx;
  long unsigned int partno=0;
  long unsigned int maxfeature_per_thread = (tctx->pctx->features/pcnt)+1;
  for(int i=0; i < pcnt; i++){
    dpart[i] = (delta_entry *)calloc(cnt+10, sizeof(delta_entry));
    assert(dpart[i]);
  }
  for(int i=0; i < cnt; i++){
    partno = idxlist[i]/maxfeature_per_thread;
    dpart[partno][carray[partno]].idx = idxlist[i];
    dpart[partno][carray[partno]].delta = deltalist[i];
    carray[partno]++;
  }
  int localcnt=0;
  for(int i=tctx->cctx->wmachines; i<tctx->cctx->totalmachines-1 ; i++){
    for(int j=0; j<cctx->schedthrd_per_mach; j++){
      partno = cctx->schedthrd_per_mach*localcnt + j;
      //strads_msg(ERR, "APP SCHEDULER @@@@@ Partno [%d]\n", partno);
      if(carray[partno] > 0){
#if 0      
	strads_msg(INF, "[App-Sched] rank(%d) sends delta update packet to %d pktsize[%ld] \n", 
		   tctx->rank, i, sizeof(delta_entry)*carray[partno]);      	
	for(int k=0; k < carray[partno]; k++){
	  strads_msg(INF, "\t\t\t\t @@@@@ [%d]th %ld partno -- idx(%ld) \n", 
		     k, partno, dpart[partno][k].idx);
	}
#endif 
	strads_msg(INF, "\t\t @ sendamocomhandler: size: %ld i(%d) partno(%ld)  dpart[partno]=(%p)\n", 
		   sizeof(delta_entry)*carray[partno], i, partno, (uint8_t *)dpart[partno]);	
	/*
       	syscom_send_amo_comhandler(tctx->comh[SGR], tctx, (uint8_t *)dpart[partno], sizeof(delta_entry)*carray[partno], 
				   deltaupdate, i, partno);      
	*/
	dpart[partno][carray[partno]].idx = MIN_UNIT_CHANGE_SIGNATURE;
	dpart[partno][carray[partno]].delta = newminunit;
	carray[partno]++;
	syscom_send_amo_comhandler(tctx->comh[SGR], tctx, (uint8_t *)dpart[partno], sizeof(delta_entry)*carray[partno], 
				   deltaupdate, i, partno);      
      }
    }
    localcnt++;
  }
  for(int i=0; i < pcnt; i++){
    free(dpart[i]);
  }
  free(dpart);
  free(carray);
}

// you set deltas to NULL, it will start uniform scheduling with uniform deltas 
void psched_start_scheduling(deltaset *deltas, threadctx *tctx, double newminunit){
  if(deltas != NULL){
    psched_process_delta_update(deltas, tctx, newminunit);
  }else{
    clusterctx *cctx = tctx->cctx;
    deltaset *deltas = (deltaset *)calloc(1, sizeof(deltaset));
    uint64_t partitions = (cctx->schedthrd_per_mach*(cctx->schedmachines-1));
    uint64_t spos = (tctx->pctx->features/partitions)+1;
    for(uint64_t i=0; i < partitions; i++){
      deltas->idxlist[i] = i*spos;
      deltas->deltalist[i] = 0.0;
    }
    deltas->size = partitions; 
    psched_process_delta_update(deltas, tctx, newminunit);
    free(deltas);
  }
}

void psched_send_token(threadctx *tctx, uint64_t partno){
  clusterctx *cctx = tctx->cctx;
  uint64_t machid = (partno/cctx->schedthrd_per_mach); // local scheduler machine id 
  uint64_t dstmachine = machid + tctx->cctx->wmachines; // absolute machine id 
  syscom_send_empty_amo_comhandler(tctx->comh[SGR], tctx, NULL, 0, deltaupdate, dstmachine, partno);	
}
#endif 
