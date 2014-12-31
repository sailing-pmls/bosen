#pragma once 

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

#include <strads/include/common.hpp>
#include <strads/netdriver/zmq/zmq-common.hpp>
#include <glog/logging.h>

void *scheduler_mach(void *);

class scheduler_machctx{

public:
  scheduler_machctx(sharedctx *shctx);
  sharedctx *m_shctx;
private:
};



#if 0 

class coordinator_threadctx{

public:
  coordinator_threadctx(int rank, int coordinatormid, int threadid, sharedctx *shctx): m_shctx(shctx), m_created(false), m_mutex(PTHREAD_MUTEX_INITIALIZER), m_upsignal(PTHREAD_COND_INITIALIZER), m_rank(rank), m_coordinator_mid(coordinatormid), m_coordinator_thrdid(threadid), m_inq_lock(PTHREAD_MUTEX_INITIALIZER), m_outq_lock(PTHREAD_MUTEX_INITIALIZER), m_schedq_lock(PTHREAD_MUTEX_INITIALIZER) {

    int rc = pthread_attr_init(&m_attr);
    checkResults("pthread attr init m_attr failed", rc);
    rc = pthread_create(&m_thid, &m_attr, coordinator_thread, (void *)this);
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

  // caveat: precondition: cmd should be allocated structued eligible for free().
  void put_entry_schedq(void *cmd){
    int rc = pthread_mutex_lock(&m_schedq_lock);
    checkResults("pthread mutex lock m inq lock failed ", rc);    
    m_schedq.push_back(cmd);    
    rc = pthread_mutex_unlock(&m_schedq_lock);
    checkResults("pthread mutex lock m inq unlock failed ", rc);
  }

  // caveat: if nz returned to a caller, the caller should free nz structure 
  void *get_entry_schedq(void){
    int rc = pthread_mutex_lock(&m_schedq_lock);
    void *ret = NULL;
    checkResults("pthread mutex lock m_schedq_lock failed ", rc);

    if(!m_schedq.empty()){
      ret = m_schedq.front();
      m_schedq.pop_front();
    }    
    rc = pthread_mutex_unlock(&m_schedq_lock);
    checkResults("pthread mutex lock m_schedq_unlock failed ", rc);   
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

  inter_threadq m_schedq;
  pthread_mutex_t m_schedq_lock;

};
#endif 
