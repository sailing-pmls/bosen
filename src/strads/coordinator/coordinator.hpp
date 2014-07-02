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
/********************************************************
   @author: Jin Kyu Kim (jinkyuk@cs.cmu.edu)

   Distributed scheduler framework 
********************************************************/
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
#include <deque>
#include <unordered_map>

#include "./include/common.hpp"
#include "com/comm.hpp"
#include "com/zmq/zmq-common.hpp"
#include <glog/logging.h>

void *coordinator_mach(void *);

void *coordinator_thread(void *arg); 

void *coordinator_gc(void *arg); 



#if 1
typedef struct{

  int gthrdid;
  int schedmid;
  int64_t entrycnt;
  int64_t *amo_gtaskids;
  int64_t clock;
}stmwork;

typedef struct{
  std::unordered_map<int64_t, idmvals_pair *>*retmap;
  int64_t clock;
}stret;

enum coordcmd_type{ m_coord_statupdate, m_coord_paramupdate, m_coord_object, m_coord_gc };

class coord_cmd{

public:
  coord_cmd(coordcmd_type type, int gthrdid, int schedmid, int64_t entrycnt, int64_t *list, int64_t clock, int64_t cmdid)
    : m_type(type), m_work(NULL), m_result(NULL), m_cmdid(cmdid){

    m_type = type;
    stmwork *tmpwork = (stmwork *)calloc(1, sizeof(stmwork));
    tmpwork->amo_gtaskids = (int64_t *)calloc(entrycnt, sizeof(int64_t));
    for(int64_t i=0; i < entrycnt; i++){
      tmpwork->amo_gtaskids[i] = list[i];
    }
    tmpwork->entrycnt = entrycnt;
    tmpwork->clock = clock;
    tmpwork->gthrdid = gthrdid;
    tmpwork->schedmid = schedmid;

    m_work = tmpwork;
  }

  coord_cmd(coordcmd_type type, std::unordered_map<int64_t, idmvals_pair *> *mptr, int64_t clock, int64_t cmdid)
    : m_type(type), m_work(NULL), m_result(NULL), m_cmdid(cmdid){

    m_type = type;
    stret *tmpret = (stret *)calloc(1, sizeof(stret));
    tmpret->clock = clock;
    tmpret->retmap = mptr;
    m_result = tmpret;
  }

  coord_cmd(coordcmd_type type, int64_t clock, int64_t cmdid)
    : m_type(type), m_work(NULL), m_result(NULL), m_clock(clock), m_cmdid(cmdid){
  }


  ~coord_cmd(){   
    // add free code here 
    if(m_work != NULL){
      stmwork *tmp = (stmwork *)m_work;
      assert(tmp->amo_gtaskids != NULL);
      free(tmp->amo_gtaskids);     
      free(m_work);     
    }

    if(m_result != NULL){          
      //      stret *mret = (stret *)m_result;
      //      delete mret->retmap;
      //TODO THINK ABOUT HOW TO PREVENT MEMORY LEAK

      free(m_result);
    }
  }

  int m_workergid;
  coordcmd_type m_type;
  void *m_work;    // when mach to thread, no NULL,    when thread to mach, should be NULL
  void *m_result; // when mach to thread, should be NULL,  when thread to mach, no NULL
  int64_t m_clock;
  int64_t m_cmdid;

};
#endif 






#if 0 
class coordinator_threadctx{

public:
  coordinator_threadctx(int rank, int coordinatormid, int threadid, sharedctx *shctx): m_shctx(shctx), m_created(false), m_mutex(PTHREAD_MUTEX_INITIALIZER), m_upsignal(PTHREAD_COND_INITIALIZER), m_rank(rank), m_coordinator_mid(coordinatormid), m_coordinator_thrdid(threadid), m_inq_lock(PTHREAD_MUTEX_INITIALIZER), m_outq_lock(PTHREAD_MUTEX_INITIALIZER) {

    int rc = pthread_attr_init(&m_attr);
    checkResults("pthread attr init m_attr failed", rc);
    rc = pthread_create(&m_thid, &m_attr, coordinator_thread, (void *)this);
    checkResults("pthread create failed in scheduler_threadctx", rc);
    m_created = true;  

  };


  coordinator_threadctx(int rank, int coordinatormid, int threadid, sharedctx *shctx, bool gcflag): m_shctx(shctx), m_created(false), m_mutex(PTHREAD_MUTEX_INITIALIZER), m_upsignal(PTHREAD_COND_INITIALIZER), m_rank(rank), m_coordinator_mid(coordinatormid), m_coordinator_thrdid(threadid), m_inq_lock(PTHREAD_MUTEX_INITIALIZER), m_outq_lock(PTHREAD_MUTEX_INITIALIZER) {


    assert(gcflag);
    int rc = pthread_attr_init(&m_attr);
    checkResults("pthread attr init m_attr failed", rc);
    rc = pthread_create(&m_thid, &m_attr, coordinator_gc, (void *)this);
    checkResults("pthread create failed in scheduler_threadctx", rc);
    m_created = true;  

  };

  // caveat: precondition: cmd should be allocated structued eligible for free().
  void put_entry_inq(void *cmd){
    int rc = pthread_mutex_lock(&m_inq_lock);
    checkResults("pthread mutex lock m inq lock failed ", rc);
    if(m_inq.empty()){
      rc = pthread_cond_signal(&m_upsignal);
      checkResults("pthread cond signal failed ", rc);
    }
    m_inq.push_back(cmd);    
    rc = pthread_mutex_unlock(&m_inq_lock);
    checkResults("pthread mutex lock m inq unlock failed ", rc);
  }

  // caveat: if nz returned to a caller, the caller should free nz structure 
  void *get_entry_inq_blocking(void){
    int rc = pthread_mutex_lock(&m_inq_lock);
    void *ret = NULL;
    checkResults("pthread mutex lock m_outq_lock failed ", rc);

    if(!m_inq.empty()){
      ret = m_inq.front();
      m_inq.pop_front();
    }else{
      pthread_cond_wait(&m_upsignal, &m_inq_lock); // when waken up, it will hold the lock. 
      ret = m_inq.front();
      m_inq.pop_front();
    }

    rc = pthread_mutex_unlock(&m_inq_lock);
    checkResults("pthread mutex lock m_outq_unlock failed ", rc);   
    return ret;
  }

  // caveat: precondition: cmd should be allocated structued eligible for free().
  void put_entry_outq(void *cmd){
    int rc = pthread_mutex_lock(&m_outq_lock);
    checkResults("pthread mutex lock m inq lock failed ", rc);    
    m_outq.push_back(cmd);    
    rc = pthread_mutex_unlock(&m_outq_lock);
    checkResults("pthread mutex lock m inq unlock failed ", rc);
  }

  // caveat: if nz returned to a caller, the caller should free nz structure 
  void *get_entry_outq(void){
    int rc = pthread_mutex_lock(&m_outq_lock);
    void *ret = NULL;
    checkResults("pthread mutex lock m_outq_lock failed ", rc);

    if(!m_outq.empty()){
      ret = m_outq.front();
      m_outq.pop_front();
    }    
    rc = pthread_mutex_unlock(&m_outq_lock);
    checkResults("pthread mutex lock m_outq_unlock failed ", rc);   
    return ret;
  }

  int get_rank(void){ return m_rank; }
  int get_coordinator_mid(void){ return m_coordinator_mid; }
  int get_coordinator_thrdid(void){ return m_coordinator_thrdid; }

  sharedctx *m_shctx;

private:

  bool m_created;
  pthread_mutex_t m_mutex;
  pthread_cond_t m_upsignal;

  int m_rank;
  int m_coordinator_mid;
  int m_coordinator_thrdid; // global thread id. 

  inter_threadq m_inq;
  inter_threadq m_outq;

  pthread_mutex_t m_inq_lock;
  pthread_mutex_t m_outq_lock;

  pthread_t m_thid;
  pthread_attr_t m_attr; 

};

#endif 
