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
#pragma once 

#include <stdint.h>
#include <iostream>
#include <iterator>     // std::iterator, std::input_iterator_tag
#include <vector>
#include <algorithm>
#include <cassert>
#include "./app/dlasso/usermsg.hpp"
#include <string>
#include <assert.h>
#include "./ds/dshard.hpp"
#include "./include/common.hpp"
#include "./include/vsysfuncs.hpp"

DECLARE_double(beta);

#define DECLARE_FUNCTION(ctxp) \
  lassofunc < usermsg_two<uload<task>, uload<status>>, usermsg_one<uload<result>>, usermsg_one<uload<result>>, usermsg_one<uload<result>> > *handler = \
    new lassofunc < usermsg_two<uload<task>, uload<status>>, usermsg_one<uload<result>>, usermsg_one<uload<result>>, usermsg_one<uload<result>> >(ctxp); \
  usermsg_one<uload<result>> *pretmsg = NULL; \

// sysmsg_one<task> is system provided type for scheduling
// T1 : for dispatching  from coordinator to worker
// T2 : partial results from worker to coordinator
// T3 : for relaying status change in coordinator 
template <typename T1, typename T2, typename T3, typename T4>
class lassofunc: public basefunction{
protected:
public:

  lassofunc(sharedctx *shctx):basefunction(shctx){
  }

  void set_initial_priority(double *prioritylist, int64_t mpcnt){

    dshardctx *coeffshard = findshard("coeffRow");
    double *beta = coeffshard->m_dmat.m_mem[0];
    for(int64_t mi=0; mi < mpcnt; mi++){                                                                               
      prioritylist[mi] = beta[mi];        
    }        
  }

  void do_work(T1 &msg, sharedctx &shctx, T2 &retmsg){
    dshardctx *shardA = findshard("Arowcol");
    dshardctx *shardRes = findshard("Yrow");
    int64_t variables = msg.umember1.size_;
    for(int64_t i=0; i < variables; i++){
      int64_t vid = msg.umember1[i].idx;
      double coeff = msg.umember1[i].value;
      double thsum = 0;
      double xsq = 0;
      spmat_vector *ccol = &shardA->m_cm_vmat.col((uint64_t)vid);
      for(uint64_t idx=0; idx<ccol->size(); idx++){
	uint64_t tmprow = ccol->idx[idx];
	double tmpval = ccol->val[idx];
	if(tmprow < shardA->m_range.r_start){
	  strads_msg(ERR, " out of range in partial sum update \n");
	  exit(0);
	}
	if(tmprow > shardA->m_range.r_end){
	  strads_msg(ERR, " out of range in partial sum update \n");
	  exit(0);
	}
	thsum += ((coeff*tmpval*tmpval ) + tmpval*shardRes->m_atomic[tmprow]);
	xsq += (tmpval * tmpval);
      }
      retmsg.umember1[i].idx = vid;
      retmsg.umember1[i].psum = thsum;
      retmsg.umember1[i].sqpsum = xsq;                             
    }
  } // called at worker, defined by user 

  void *calc_scheduling(void){
    return NULL;
  } // deployed at scheduler's scheduling service 

  void update_weight(sysmsg_one<uload<task>> &taskmsg, T3 &pretmsg){
    assert(pretmsg.umember1.size_ == taskmsg.umember1.size_);
    assert(pretmsg.umember1.size_ == taskmsg.entrycnt);
    for(int i=0; i < taskmsg.entrycnt; i++){
      assert(taskmsg.umember1[i].idx == pretmsg.umember1[i].idx);
      taskmsg.umember1[i].value = pretmsg.umember1[i].diff;
    }   
  }

  T1 *dispatch_scheduling(sysmsg_one<uload<task>> &taskmsg, T3 **ppretmsg){
    T3 *pretmsg = *ppretmsg;
    int prevbetacnt;
    if(pretmsg != NULL){
      prevbetacnt = pretmsg->umember1.size_;
    }else{
      prevbetacnt = 0;
    }

    dshardctx *shardcoeff = findshard("coeffRow");
    double *beta = shardcoeff->m_dmat.m_mem[0];

    T1 *pmsg2 = new T1(USER_UW, taskmsg.entrycnt, prevbetacnt);

    for(int i=0; i < taskmsg.entrycnt; i++){
      int64_t id = taskmsg.umember1[i].idx;
      pmsg2->umember1[i].idx = id;
      pmsg2->umember1[i].value = beta[id];
    }

    for(int i=0; i < prevbetacnt; i++){
      pmsg2->umember2[i].idx = pretmsg->umember1[i].idx;
      pmsg2->umember2[i].value = pretmsg->umember1[i].diff; // beta diff is update in user defined fuction                
    }

    assert(pmsg2->get_blen() + 64 < (long)sizeof(mbuffer)); // TODO Replace this 64                              
    char *tmp = pmsg2->serialize();
    mcopy_broadcast_to_workers(m_shctx, tmp, pmsg2->get_blen());
    free(tmp); // free temporary allocated message by serialize 
    delete pmsg2;

    if(pretmsg != NULL)
      delete pretmsg ;

    // pretmsg : old previous one                                                                                                 
    pretmsg = new usermsg_one<uload<result>>(USER_UW_RES, taskmsg.entrycnt);
    for(int i=0; i < taskmsg.entrycnt; i++){
      pretmsg->umember1[i].idx = taskmsg.umember1[i].idx;
      pretmsg->umember1[i].psum = 0;
      pretmsg->umember1[i].sqpsum = 0;
      pretmsg->umember1[i].diff = 0;
    }
    *ppretmsg = pretmsg;

    return NULL;
  } // called at coordinator, defined by the system
  
  // combine partialmsgs coming from worker machines into a retmsg 
  void do_msgcombiner(T2 &retmsg, sharedctx *ctx){
    mbuffer **bufs = ctx->make_msgcollection(); 
    for(int i=0; i < m_shctx->m_worker_machines; i++){
      mbuffer *mbuf = (mbuffer *)bufs[i];
      usermsg_one<uload<result>> *prmsg1;
      prmsg1 = (usermsg_one<uload<result>> *)mbuf->data;
      prmsg1->deserialize(mbuf->data, -1);
      for(int64_t i =0; i < prmsg1->umember1.size_; i++){
	assert(retmsg.umember1[i].idx == prmsg1->umember1[i].idx);
	retmsg.umember1[i].psum += prmsg1->umember1[i].psum;
	retmsg.umember1[i].sqpsum += prmsg1->umember1[i].sqpsum;
      }
    }
    ctx->release_msgcollection(bufs);
  }

  void *do_aggregate(T2 &retmsg, sharedctx *ctx){

    double lambda = FLAGS_beta;
    dshardctx *shardcoeff = findshard("coeffRow");
    double *beta = shardcoeff->m_dmat.m_mem[0];
    dshardctx *shardcoeffdiff = findshard("coeffdiff");
    double *betadiff = shardcoeffdiff->m_dmat.m_mem[0];

    for(int64_t i =0; i < retmsg.umember1.size_; i++){
      int id = retmsg.umember1[i].idx;
      double tmp = _soft_threshold(retmsg.umember1[i].psum, lambda/2.0);
      double newcoeff = tmp / retmsg.umember1[i].sqpsum;
      //double oldbeta = beta[id];
      betadiff[id] = beta[id] - newcoeff;
      retmsg.umember1[i].diff = beta[id] - newcoeff;
      beta[id] = newcoeff;
    }
    return NULL;
  } // called at coordinator, defined by user

  void *update_status(T1 &msg, sharedctx &shctx){
    dshardctx *shardA = findshard("Arowcol");
    dshardctx *shardRes = findshard("Yrow");
    int64_t updatecnt = msg.umember2.size_;
    for(int64_t i=0; i < updatecnt; i++){
      int64_t idx = msg.umember2[i].idx;
      double delta = msg.umember2[i].value;
      double rdelta = 0;
      if(delta != 0.0){
	spmat_vector *ccol = &shardA->m_cm_vmat.col((uint64_t)idx);
	for(long unsigned int rcnt = 0; rcnt < ccol->size(); rcnt++){
	  uint64_t tmprow = ccol->idx[rcnt];
	  double tmpval = ccol->val[rcnt];
	  rdelta = tmpval * delta;
	  shardRes->m_atomic.add(tmprow, rdelta);
	}
      }
    }
    return NULL;
  }// called at worker, defined by user
  
  /* soft_thrd, multiresidual  for workers */
  double _soft_threshold(double sum, double lambda){
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

  int64_t check_interference(int64_t *gidxsamples, int64_t size, dshardctx *dshard, int64_t m_base, double m_infthreshold){    
#if !defined(COL_SORT_INPUT)
    assert(0);
#endif 
    assert(dshard->m_type == cvspt);
    col_vspmat *cm_vmat = &(dshard->m_cm_vmat);
    strads_msg(INF, "Check Inference with this matrix cmvmat %p \n", cm_vmat);
    int64_t gvidsample=0, spos=0;
    int64_t gvidret=0, chkpos=0, rvidxsize=0;
    int64_t *rvidx=(int64_t *)calloc(size, sizeof(int64_t));
    double infdegree=0;
    bool conflict=false;
    rvidx[0] = gidxsamples[0];
    rvidxsize=1;
    for(spos=1; spos < size; spos++){
      gvidsample = gidxsamples[spos];  
      conflict=false;
      for(chkpos = 0; chkpos < rvidxsize; chkpos++){
	assert(rvidxsize < size);
	gvidret = rvidx[chkpos];
	infdegree = getcorr_pair_sparse_vmat(cm_vmat->col(gvidsample), cm_vmat->col(gvidret), dshard->m_rows, 
					     0, 0, 0, 0, 0, gvidsample, gvidret); // lastcorr.cpp	    
	// use system provided correlation calc routine that works on sparse vector matrix 

	strads_msg(INF, "interference between (%ld) (%ld)  == %lf \n", gvidsample, gvidret, infdegree);
	if(infdegree >= m_infthreshold){
	  conflict = true;
	  break;
	}
      }
      if(!conflict){ // if false - does not cause conflicts
	rvidx[rvidxsize] = gvidsample;
	rvidxsize++;
      }
      if(rvidxsize >= size){
	break;
      }		   
    }
    for(int64_t i=0; i < rvidxsize; i++){
      gidxsamples[i] = rvidx[i];
    }
    free(rvidx);
    return rvidxsize;
  }  

  void do_obj_calc(T4 &msg, sharedctx &shctx){
    dshardctx *resdshard = findshard("Yrow");
    uint64_t sample_start = resdshard->m_range.r_start;
    uint64_t sample_end = resdshard->m_range.r_end;
    double pobjsum=0;
    for(uint64_t i=sample_start; i<= sample_end; i++){
      pobjsum += (resdshard->m_atomic[i]*resdshard->m_atomic[i]);
    }
    msg.umember1[0].idx= -1;
    msg.umember1[0].sqpsum = pobjsum;                                 
  } // called at worker, defined by user 

  // combine partialmsgs coming from worker machines into a retmsg 
  void do_msgcombiner_obj(T4 &retmsg, sharedctx *ctx){
    mbuffer **bufs = ctx->make_msgcollection(); 
    retmsg.umember1[0].sqpsum = 0;
    for(int i=0; i < m_shctx->m_worker_machines; i++){
      mbuffer *mbuf = (mbuffer *)bufs[i];
      usermsg_one<uload<result>> *prmsg1;
      prmsg1 = (usermsg_one<uload<result>> *)mbuf->data;
      prmsg1->deserialize(mbuf->data, -1);
      assert(prmsg1->umember1.size_ == 1);
      for(int64_t j =0; j < prmsg1->umember1.size_; j++){
	retmsg.umember1[0].sqpsum += prmsg1->umember1[j].sqpsum;
      }
    }
    ctx->release_msgcollection(bufs);
  }

  double do_object_aggregate(T4 &retmsg, sharedctx *ctx){
    dshardctx *shardcoeff = findshard("coeffRow");
    double *beta = shardcoeff->m_dmat.m_mem[0];

    double betasum=0;
    int64_t nzcnt=0;
    int64_t modelsize = ctx->m_params->m_sp->m_modelsize;
    double lambda = FLAGS_beta;

    for(int64_t i=0; i<modelsize; i++){
      betasum += fabs(beta[i]);
      if(beta[i] != 0){
	nzcnt++;
      }
    }
    double psum = retmsg.umember1[0].sqpsum;
    double objvalue = psum + lambda*betasum;
    if(std::isnan(objvalue)){
      strads_msg(ERR, "NAN ERROR psum : %lf, betasum : %lf\n", psum, betasum);
      assert(0);
      exit(-1);
    }
    strads_msg(OUT, "\t\t Object %lf nz(%ld)\n",
	       objvalue/2.0, nzcnt);

    return objvalue/2.0;
    //    return NULL;
  } // called at coordinator, defined by user

  void *log_parameters(sharedctx *ctx){
    dshardctx *shardcoeff = findshard("coeffRow");
    double *beta = shardcoeff->m_dmat.m_mem[0];
    int64_t modelsize = ctx->m_params->m_sp->m_modelsize;
    for(int64_t i=0; i<modelsize; i++){
      if(beta[i] != 0.0)
	ctx->write_beta(i, beta[i]);
    }
    ctx->flush_beta();
    return NULL;
  } 

};
