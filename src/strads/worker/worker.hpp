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

class work_cmd;
class worker_threadctx;

#include "./include/common.hpp"
#include "com/comm.hpp"
#include "com/zmq/zmq-common.hpp"
#include <glog/logging.h>

void *worker_mach(void *);
void *worker_thread(void *arg); 
void *whalf_thread(void *arg);

enum workcmd_type{ m_work_statupdate, m_work_paramupdate, m_work_object };

class work_cmd{

public:

  work_cmd(workcmd_type type): m_type(type), m_work(NULL), m_result(NULL), m_temptaskcnt(-1), m_cmdid(-1){
    m_type = type;
  }

  work_cmd(workcmd_type type, int64_t temptaskcnt, int64_t cmdid, message_type msgtype) 
    : m_type(type), m_work(NULL), m_result(NULL), m_temptaskcnt(temptaskcnt), m_cmdid(cmdid), m_msgtype(msgtype){
    m_type = type;
  }

  ~work_cmd(){   
    if(m_work != NULL){
      free(m_work);     
    }
    if(m_result != NULL){
      free(m_result);
    }
  }

  int m_workergid;
  workcmd_type m_type;
  void *m_work;    // when mach to thread, no NULL,    when thread to mach, should be NULL
  void *m_result; // when mach to thread, should be NULL,  when thread to mach, no NULL
  int64_t m_temptaskcnt;
  int64_t m_cmdid;
  message_type m_msgtype;
  double m_svm_obj_tmp;
};

class worker_threadctx{

public:
  worker_threadctx(int rank, int workermid, int threadid, pthread_mutex_t *pmutex, int64_t *freethrds, parameters *mparam, int workermachines, sharedctx *shctx)
  : m_pmutex(pmutex), m_freethrds(freethrds), m_params(mparam), m_rank(rank), m_created(false), m_mutex(PTHREAD_MUTEX_INITIALIZER), 
    m_upsignal(PTHREAD_COND_INITIALIZER),  m_worker_mid(workermid), m_worker_thrdlid(threadid), 
    m_inq_lock(PTHREAD_MUTEX_INITIALIZER), m_outq_lock(PTHREAD_MUTEX_INITIALIZER), m_shctx(shctx) {

    int rc = pthread_attr_init(&m_attr);
    checkResults("pthread attr init m_attr failed", rc);
    rc = pthread_create(&m_thid, &m_attr, worker_thread, (void *)this);
    checkResults("pthread create failed in scheduler_threadctx", rc);
    m_created = true;  

    m_worker_machines = workermachines; 
    m_thrds_per_worker = mparam->m_sp->m_thrds_per_worker;
    
    m_worker_thrdgid = workermid * m_thrds_per_worker + threadid ; // local thread id. 
  };

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

  void *get_entry_inq_blocking(void){
    int rc = pthread_mutex_lock(&m_inq_lock);
    void *ret = NULL;
    checkResults("pthread mutex lock m_outq_lock failed ", rc);

    if(!m_inq.empty()){
      ret = m_inq.front();
      m_inq.pop_front();
    }else{

#if 1 // test code 
      pthread_mutex_lock(m_pmutex);
      (*m_freethrds)++;
      pthread_mutex_unlock(m_pmutex);
#endif 
      pthread_cond_wait(&m_upsignal, &m_inq_lock); // when waken up, it will hold the lock. 
#if 1 // test code pair with above test code 
      pthread_mutex_lock(m_pmutex);
      (*m_freethrds)--;
      assert(m_freethrds >= 0);
      pthread_mutex_unlock(m_pmutex);
#endif 
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
  int get_worker_mid(void){ return m_worker_mid; }
  int get_worker_thrdlid(void){ return m_worker_thrdlid; }
  int get_worker_thrdgid(void){ return m_worker_thrdgid; }
  sharedctx *get_shctx(void) { return m_shctx; }

  pthread_mutex_t *m_pmutex; // declared outside
  int64_t *m_freethrds; // points to one glboal counter declared outside 
  parameters *m_params;
  int m_worker_machines;
  int m_thrds_per_worker;
  int m_rank;

private:

  bool m_created;
  pthread_mutex_t m_mutex;
  pthread_cond_t m_upsignal;
  int m_worker_mid;
  int m_worker_thrdlid; // local thread id. 
  int m_worker_thrdgid; // local thread id. 
  inter_threadq m_inq;
  inter_threadq m_outq;
  pthread_mutex_t m_inq_lock;
  pthread_mutex_t m_outq_lock;
  pthread_t m_thid;
  pthread_attr_t m_attr; 
  sharedctx *m_shctx;
};
