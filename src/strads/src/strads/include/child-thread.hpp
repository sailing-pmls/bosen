#pragma once 

#include<pthread.h>
#include<strads/include/indepds.hpp>

// CAVEAT : big assumption: scheduler_thread / wsamplerctx are not thread safe.                                          
//          Only one thread should access these classes                                                                 
//          Since each scheduler threads are independent, this is reasonable for current desig                              
class child_thread{
public:
  child_thread(int rank, int thid, void *(*child_func)(void *), void *userarg): 
    m_created(false), 
    m_userarg(userarg),
    m_upsignal(PTHREAD_COND_INITIALIZER), 
    m_rank(rank), 
    m_thid(thid),
    m_inq_lock(PTHREAD_MUTEX_INITIALIZER), 
    m_outq_lock(PTHREAD_MUTEX_INITIALIZER),
    m_child_func(child_func){

    int rc = pthread_attr_init(&m_attr);
    checkResults("pthread attr init m_attr failed", rc);
    rc = pthread_create(&m_pthid, &m_attr, m_child_func, (void *)this);
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

  // caveat: if nz returned to a caller, the caller should free nz structure                                               
  void *get_entry_inq_nonblocking(void){
    int rc = pthread_mutex_lock(&m_inq_lock);
    void *ret = NULL;
    checkResults("pthread mutex lock m_outq_lock failed ", rc);
    if(!m_inq.empty()){
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
  int get_thid(void){ return m_thid; }
  void * get_userarg(void){ return m_userarg; }

  bool m_created;
  void *m_userarg; // user defined argument if necessary

private:
  pthread_cond_t m_upsignal;
  int m_rank;
  int m_thid;
  pthread_t m_pthid;
  pthread_mutex_t m_inq_lock;
  pthread_mutex_t m_outq_lock;

  inter_threadq m_inq;
  inter_threadq m_outq;
  pthread_attr_t m_attr;

  void *(*m_child_func)(void *);
};
