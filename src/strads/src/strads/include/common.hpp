

/**************************************************************
  @author: Jin Kyu Kim (jinkyuk@cs.cmu.edu)

***************************************************************/
#pragma once 

#include <iostream>
#include <stdio.h>
#include <map>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <assert.h>
#include <zmq.hpp>
#include <strads/include/coordinator.hpp>
#include <strads/include/worker.hpp>
#include <strads/include/scheduler.hpp>
#include <strads/include/strads-pi.hpp>
#include <strads/include/sysparam.hpp>
#include <strads/util/utility.hpp>
#include <strads/netdriver/comm.hpp>
#include <strads/netdriver/zmq/zmq-common.hpp>
#include <strads/ds/dshard.hpp>

sharedctx *strads_init(int argc, char **argv);
void *process_common_system_cmd(sharedctx *ctx, mbuffer *mbuf, context *recv_ctx, context *send_ctx, bool *tflag);
void mcopy_broadcast_to_workers(sharedctx *ctx, void *tmp, int len);
void mcopy_broadcast_to_workers_objectcalc(sharedctx *ctx, void *tmp, int len);

class task_assignment{
public:
  std::map<int, range *>schmach_tmap;
  std::map<int, range *>schthrd_tmap;
};

typedef struct {
  int64_t size;
  int64_t phaseno;
  uobj_type  type; // user defined type that allows user to parse the data entry 
  void *data;
}staleinfo;


//#define IHWM (10)
#define IHWM (MAX_ZMQ_HWM - 5)

// This class does not have declaration of destructor 
//  since it will not be deleted as far as the program runs 
class sharedctx {

public:
  sharedctx(const int r, sysparam *sp):
    rank(r), m_mrole(mrole_unknown), m_sport_lock(PTHREAD_MUTEX_INITIALIZER), m_rport_lock(PTHREAD_MUTEX_INITIALIZER), 
    m_sport_flag(false), m_rport_flag(false), ring_sport(NULL), ring_rport(NULL), m_fsport_lock(PTHREAD_MUTEX_INITIALIZER), 
    m_frport_lock(PTHREAD_MUTEX_INITIALIZER), m_fsport_flag(false), m_frport_flag(false), ring_fsport(NULL), ring_frport(NULL), 
    m_starsport_lock(PTHREAD_MUTEX_INITIALIZER), m_starrport_lock(PTHREAD_MUTEX_INITIALIZER),  m_starsend(PTHREAD_MUTEX_INITIALIZER),
    m_starrecv(PTHREAD_MUTEX_INITIALIZER), m_sp(sp), m_idvalp_buf(NULL), m_freethrds_lock(PTHREAD_MUTEX_INITIALIZER), 
    m_freethrds(0), m_schedctx(NULL), m_coordctx(NULL), m_workerctx(NULL), m_ringtoken_send_lock(PTHREAD_MUTEX_INITIALIZER), 
    m_ringtoken_recv_lock(PTHREAD_MUTEX_INITIALIZER), 
    m_upsignal_syncput(PTHREAD_COND_INITIALIZER), 
    m_upsignal_syncget(PTHREAD_COND_INITIALIZER), 
    m_lock_syncput(PTHREAD_MUTEX_INITIALIZER), 
    m_lock_syncget(PTHREAD_MUTEX_INITIALIZER),
    m_lock_async_count(PTHREAD_MUTEX_INITIALIZER),
    __ctx(this), 
    sub_comm_ptr(NULL){

    m_scheduler_mid = -1; 
    m_worker_mid = -1;    
    m_coordinator_mid = -1; 

    m_zmqcontext=NULL;

    m_mrole = mrole_unknown;
    m_ringtoken_send = IHWM;
    m_ringtoken_recv = 0;

    ps_callback_func = NULL;
    ps_server_pgasyncfunc=NULL;
    ps_server_putsyncfunc=NULL;
    ps_server_getsyncfunc=NULL;

    m_tablectx = 0;;
    m_max_pend = MAX_ZMQ_HWM/10;
    assert(m_max_pend > 10); // too small increase max amq hwm 
    m_async_count = 0;

    strads_msg(ERR, "@@@@@@@@@@@@@@@@@@@@@@@@@@@ M MAX PEND : %ld init count  %ld @@@@@@@@@@@@@2 \n", 
	       m_max_pend, m_async_count); 
  };

  sharedctx(const int r, sysparam *sp, const int mpisize):
    rank(r), m_mrole(mrole_unknown), m_sport_lock(PTHREAD_MUTEX_INITIALIZER), m_rport_lock(PTHREAD_MUTEX_INITIALIZER), 
    m_sport_flag(false), m_rport_flag(false), ring_sport(NULL), ring_rport(NULL), m_fsport_lock(PTHREAD_MUTEX_INITIALIZER), 
    m_frport_lock(PTHREAD_MUTEX_INITIALIZER), m_fsport_flag(false), m_frport_flag(false), ring_fsport(NULL), ring_frport(NULL), 
    m_starsport_lock(PTHREAD_MUTEX_INITIALIZER), m_starrport_lock(PTHREAD_MUTEX_INITIALIZER),  m_starsend(PTHREAD_MUTEX_INITIALIZER),
    m_starrecv(PTHREAD_MUTEX_INITIALIZER), m_sp(sp), m_idvalp_buf(NULL), m_freethrds_lock(PTHREAD_MUTEX_INITIALIZER), 
    m_freethrds(0), m_schedctx(NULL), m_coordctx(NULL), m_workerctx(NULL), m_ringtoken_send_lock(PTHREAD_MUTEX_INITIALIZER), 
    m_ringtoken_recv_lock(PTHREAD_MUTEX_INITIALIZER), 
    m_upsignal_syncput(PTHREAD_COND_INITIALIZER), 
    m_upsignal_syncget(PTHREAD_COND_INITIALIZER), 
    m_lock_syncput(PTHREAD_MUTEX_INITIALIZER), 
    m_lock_syncget(PTHREAD_MUTEX_INITIALIZER),
    m_lock_async_count(PTHREAD_MUTEX_INITIALIZER),
    __ctx(this), 
    sub_comm_ptr(NULL){

    m_scheduler_mid = -1; 
    m_worker_mid = -1;    
    m_coordinator_mid = -1; 

    m_zmqcontext=NULL;

    m_mrole = mrole_unknown;
    m_ringtoken_send = IHWM;
    m_ringtoken_recv = 0;

    ps_callback_func = NULL;
    ps_server_pgasyncfunc=NULL;
    ps_server_putsyncfunc=NULL;
    ps_server_getsyncfunc=NULL;

    m_tablectx = 0;;
    m_max_pend = MAX_ZMQ_HWM/10;
    assert(m_max_pend > 10); // too small increase max amq hwm 
    m_async_count = 0;

    m_mpi_size = mpisize;
    strads_msg(ERR, "@@@@@@@@@@@@@@@@@@@@@@@@@@@ M MAX PEND : %ld init count  %ld @@@@@@@@@@@@@2 \n", 
	       m_max_pend, m_async_count); 
  };



  sharedctx():
    rank(-1){};

  std::map<int, mnode *> nodes; // machine nodes
  std::map<int, mlink *> links; // node graph links
  std::map<int, mlink *> rlinks; // node graph links
  int rank;                     // machine id
  mach_role m_mrole;

  void *(*machine_func)(void *); 

  void set_sport_flag(bool flag){
    pthread_mutex_lock(&m_sport_lock);
    m_sport_flag = flag;
    pthread_mutex_unlock(&m_sport_lock);
  }

  void set_rport_flag(bool flag){
    pthread_mutex_lock(&m_rport_lock);
    m_rport_flag = flag;
    pthread_mutex_unlock(&m_rport_lock);
  }

  bool get_rport_flag(void){
    bool ret;
    pthread_mutex_lock(&m_rport_lock);
    ret = m_rport_flag;
    pthread_mutex_unlock(&m_rport_lock);
    return ret;
  }

  bool get_sport_flag(void){
    bool ret;
    pthread_mutex_lock(&m_sport_lock);
    ret = m_sport_flag;
    pthread_mutex_unlock(&m_sport_lock);
    return ret;
  }

  void set_rport(_ringport *rport){
    pthread_mutex_lock(&m_rport_lock);
    ring_rport = rport;
    pthread_mutex_unlock(&m_rport_lock);   
  }

  void set_sport(_ringport *sport){
    pthread_mutex_lock(&m_sport_lock);
    ring_sport = sport;
    pthread_mutex_unlock(&m_sport_lock);   
  }

  // fast ring stuff ....
  void set_fsport_flag(bool flag){
    pthread_mutex_lock(&m_fsport_lock);
    m_fsport_flag = flag;
    pthread_mutex_unlock(&m_fsport_lock);
  }

  void set_frport_flag(bool flag){
    pthread_mutex_lock(&m_frport_lock);
    m_frport_flag = flag;
    pthread_mutex_unlock(&m_frport_lock);
  }

  bool get_frport_flag(void){
    bool ret;
    pthread_mutex_lock(&m_frport_lock);
    ret = m_frport_flag;
    pthread_mutex_unlock(&m_frport_lock);
    return ret;
  }

  bool get_fsport_flag(void){
    bool ret;
    pthread_mutex_lock(&m_fsport_lock);
    ret = m_fsport_flag;
    pthread_mutex_unlock(&m_fsport_lock);
    return ret;
  }

  void set_frport(_ringport *rport){
    pthread_mutex_lock(&m_frport_lock);
    ring_frport = rport;
    pthread_mutex_unlock(&m_frport_lock);   
  }

  void set_fsport(_ringport *sport){
    pthread_mutex_lock(&m_fsport_lock);
    ring_fsport = sport;
    pthread_mutex_unlock(&m_fsport_lock);   
  }

  void insert_star_recvport(uint16_t port, _ringport *ctx){
    int r = pthread_mutex_lock(&m_starrecv);
    checkResults("[insert star recvport] get lock failed", r);
    star_recvportmap.insert(std::pair<uint16_t, _ringport *>(port, ctx));
    r = pthread_mutex_unlock(&m_starrecv);   
    checkResults("[insert star recvport] release lock failed", r);
  }

  void insert_star_sendport(uint16_t port, _ringport *ctx){
    int r = pthread_mutex_lock(&m_starsend);
    checkResults("[insert star sendport] release lock failed", r);
    star_sendportmap.insert(std::pair<uint16_t, _ringport *>(port, ctx));
    pthread_mutex_unlock(&m_starsend);   
    checkResults("[insert star sendport] release lock failed", r);
  }

  unsigned int get_size_recvport(void){
    unsigned int ret;
    int r = pthread_mutex_lock(&m_starrecv);
    checkResults("[insert star recvport] get lock failed", r);
    ret = star_recvportmap.size();
    r = pthread_mutex_unlock(&m_starrecv);   
    checkResults("[insert star recvport] release lock failed", r);
    return ret;
  }

  unsigned int get_size_sendport(void){
    unsigned int ret;
    int r = pthread_mutex_lock(&m_starsend);
    checkResults("[insert star sendport] get lock failed", r);
    ret = star_sendportmap.size();
    r = pthread_mutex_unlock(&m_starsend);   
    checkResults("[insert star sendport] release lock failed", r);
    return ret;
  }

  void get_lock_sendportmap(void){
    int r = pthread_mutex_lock(&m_starsend);
    checkResults("[get lock on sendport] get lock failed", r);
  }

  void release_lock_sendportmap(void){
    int r = pthread_mutex_unlock(&m_starsend);
    checkResults("[release lock sendport] unlock failed", r);
  }

  void get_lock_recvportmap(void){
    int r = pthread_mutex_lock(&m_starrecv);
    checkResults("[get lock on recvport] get lock failed", r);
  }

  void release_lock_recvportmap(void){
    int r = pthread_mutex_unlock(&m_starrecv);
    checkResults("[release lock on recv port] unlock failed", r);
  }

  void bind_params(sysparam *param){
    assert(param);
    m_sp = param;
  }

  void make_scheduling_taskpartition(int64_t modelsize, int schedmach, int thrd_permach){

    int parts = schedmach*thrd_permach;   
    int64_t share = modelsize/parts;
    int64_t remain = modelsize % parts;
    int64_t start, end, myshare;

    //  give global assignment scheme per thread 
    for(int i=0; i < parts; i++){
      if(i==0){
	start = 0;
      }else{
	start = m_tmap.schthrd_tmap[i-1]->end + 1;
      }
      if(i < remain){
	myshare = share+1;
      }else{
	myshare = share;
      }
      end = start + myshare -1;
      if(end >= modelsize){
	end = modelsize -1;
      }
      range *tmp = new range;
      tmp->start = start;
      tmp->end = end;
      m_tmap.schthrd_tmap.insert(std::pair<int, range *>(i, tmp));			      
    }
    
    // give global assignment per mach 
    for(int i=0; i < schedmach; i++){
      int start_thrd = i*thrd_permach;
      int end_thrd = start_thrd + thrd_permach - 1;
      range *tmp = new range;
      tmp->start = m_tmap.schthrd_tmap[start_thrd]->start;
      tmp->end = m_tmap.schthrd_tmap[end_thrd]->end;
      m_tmap.schmach_tmap.insert(std::pair<int, range *>(i, tmp));
    }

    if(rank == 0){
      for(auto p : m_tmap.schthrd_tmap){
	strads_msg(ERR, "[scheduling task partitioning] scheduler thread(%d) start(%ld) end(%ld)\n", 
		   p.first, p.second->start, p.second->end); 
      }

      for(auto p : m_tmap.schmach_tmap){
	strads_msg(ERR, "[scheduling task partitioning] scheduler mach(%d) start(%ld) end(%ld)\n", 
		   p.first, p.second->start, p.second->end); 
      }
    }
  }

  mach_role find_role(int mpi_size){

    mach_role mrole = mrole_unknown;
    int sched_machines = m_sp->m_schedulers;      
    int first_schedmach = mpi_size - sched_machines - 1;
    int first_coordinatormach = mpi_size - 1; 


    m_sched_machines = m_sp->m_schedulers;      
    m_first_schedmach = mpi_size - m_sched_machines - 1;
    m_worker_machines = m_first_schedmach;
    m_first_workermach = 0;
    m_first_coordinatormach = mpi_size - 1; 


    if(rank == 0)
      strads_msg(ERR, "@@@@@@@@ FIND ROLE : Rank(%d) schedmach(%d) schedmach(%d) coordinatemach (%d)\n",
		 rank, sched_machines, first_schedmach, first_coordinatormach);

    if(rank == first_coordinatormach){
      mrole = mrole_coordinator; 
    }else if(rank < first_schedmach){       
      mrole = mrole_worker;
    }else if(rank >= first_schedmach && rank < first_coordinatormach){
      mrole = mrole_scheduler;
    }
    assert(mrole != mrole_unknown);

    strads_msg(ERR, "@@@@@@@@@@@@ My rank(%d) role (%d) [%d:coord][%d:worker][%d:sched]\n", 
	       rank, mrole, mrole_coordinator, mrole_worker, mrole_scheduler); 

    m_mrole = mrole;

    return mrole;
  }

  mach_role find_role(int mpi_size, int dstnode){

    mach_role mrole = mrole_unknown;
    int sched_machines = m_sp->m_schedulers;      
    int first_schedmach = mpi_size - sched_machines - 1;
    int first_coordinatormach = mpi_size - 1; 

    m_sched_machines = m_sp->m_schedulers;      
    m_first_schedmach = mpi_size - m_sched_machines - 1;
    m_worker_machines = m_first_schedmach;
    m_first_workermach = 0;
    m_first_coordinatormach = mpi_size - 1; 

    if(dstnode == first_coordinatormach){
      mrole = mrole_coordinator; 
    }else if(dstnode < first_schedmach){       
      mrole = mrole_worker;
    }else if(dstnode >= first_schedmach && dstnode < first_coordinatormach){
      mrole = mrole_scheduler;
    }
    assert(mrole != mrole_unknown);

    return mrole;
  }


  void prepare_machine(int mpi_size){
    assert(rank < mpi_size); // sanity check
    m_mpi_size = mpi_size;

#if defined(SVM_COL_PART)
    //    m_weights = (double *)calloc(m_params->m_up->m_samples, sizeof(double));
    //    m_weights_size = m_params->m_up->m_samples;
#endif 

    if(!m_sp->m_scheduling){
      assert(m_sp->m_schedulers == 0);
    }else{
      assert(m_sp->m_schedulers > 0);
    }

    /* assign roles to machines 
     *   first n machines are workers 
     *   next m machines are schedulers if any. this could be zero if scheduling is disabled. 
     *   last one is coordinator
     *  n + m + 1 == mpi_size 
     */
    m_sched_machines = m_sp->m_schedulers;      
    m_first_schedmach = mpi_size - m_sched_machines - 1;
    m_worker_machines = m_first_schedmach;
    m_first_workermach = 0;
    m_first_coordinatormach = mpi_size - 1; 

    if(rank == m_first_coordinatormach){
      m_mrole = mrole_coordinator; 
      //      machine_func = &coordinator_mach;
      m_coordinator_mid = rank - m_first_coordinatormach; 

      strads_msg(ERR, "[prepare_machine] rank(%d) coordinator\n", rank);
      // TODO : fill out scheduler/worekr - send/recvportmap 
    }else if(rank < m_first_schedmach){       
      m_mrole = mrole_worker;
      //      machine_func = &worker_mach;
      m_worker_mid = rank - m_first_workermach; 
      strads_msg(ERR, "[prepare_machine] rank(%d) worker\n", rank);
      // TODO : fill out coordinator send/recvportmap 
    }else if(rank >= m_first_schedmach && rank < m_first_coordinatormach){
      m_mrole = mrole_scheduler;
      //      machine_func = &scheduler_mach;
      m_scheduler_mid = rank - m_first_schedmach; 
      strads_msg(ERR, "[prepare_machine] rank(%d) scheduler\n", rank);
      // TODO : fill out coordinator send/recvportmap 
    }
    assert(m_mrole != mrole_unknown);

    if(m_mrole == mrole_coordinator){ // only coordinator need to make a log for beta, progress log, meta log files 
      assert(m_sp != NULL);
      //      int64_t iterations = m_sp->m_iter;
      //      int64_t progressfreq = m_sp->m_progressfreq;
      //      int64_t logs = iterations / progressfreq;
    }
  }

  void start_framework(void){
    pthread_attr_t attr;
    pthread_t pthid;
    int rc = pthread_attr_init(&attr);
    checkResults("pthread attr init failed\n", rc);
    rc = pthread_create(&pthid, &attr, machine_func, (void *)this);
    checkResults("pthread_create failed\n", rc);
    void *res;
    pthread_join(pthid, &res);
  }

  int get_mpi_size(void){ return m_mpi_size; }

  // data ring stuff
  pthread_mutex_t m_sport_lock; // for flag, not for port
  pthread_mutex_t m_rport_lock; // for flag, not for port
  bool m_sport_flag;
  bool m_rport_flag;
  class _ringport *ring_sport; // once initialized, no more change
  class _ringport *ring_rport; // once initialized, no more change

  // fast ring stuff
  pthread_mutex_t m_fsport_lock; // for flag, not for port
  pthread_mutex_t m_frport_lock; // for flag, not for port
  bool m_fsport_flag;
  bool m_frport_flag;
  class _ringport *ring_fsport; // once initialized, no more change
  class _ringport *ring_frport; // once initialized, no more change

  // star topology stuff -- can be used for any other topology as well. 
  pthread_mutex_t m_starsport_lock; // for flag, not for port
  pthread_mutex_t m_starrport_lock; // for flag, not for port
  bool m_starsport_flag;
  bool m_starrport_flag;
  pthread_mutex_t m_starsend; // for flag, not for port
  pthread_mutex_t m_starrecv; // for flag, not for port
  std::map<uint16_t, _ringport *> star_recvportmap;
  std::map<uint16_t, _ringport *> star_sendportmap;

  sysparam *m_sp;

  int m_scheduler_mid; // scheduler id from 0 to sizeof(schedulers)-1
  int m_worker_mid;    // worker id from 0 to sizeof(workers)-1
  int m_coordinator_mid; // coordinator id from 0 to sizeof(coordinator)-1

  task_assignment m_tmap;

  int m_sched_machines;
  int m_first_schedmach;
  int m_worker_machines;
  int m_first_workermach;
  int m_first_coordinatormach; // first one and last one since only one coordinator now
  int m_mpi_size;

  zmq::context_t *m_zmqcontext;

  // rank number - ringport map 
  std::map<int, _ringport *> scheduler_sendportmap; // coordinator
  std::map<int, _ringport *> scheduler_recvportmap; // coordinator 
  
  std::map<int, _ringport *> coordinator_sendportmap; // workers and scheduler 
  std::map<int, _ringport *> coordinator_recvportmap; // workers and scheduler 

  // star topology only: ring topology does not use this map 
  std::map<int, _ringport *> worker_sendportmap; // coordinator
  std::map<int, _ringport *> worker_recvportmap; // coordinator 

  std::map<int, _ringport *> ring_sendportmap; // coordinator
  std::map<int, _ringport *> ring_recvportmap; // coordinator 

  idval_pair *m_idvalp_buf;  
  void register_shard(dshardctx *ds){
    std::string *alias = new std::string(ds->m_alias);
    //    strads_msg(ERR, "MALIAS : %c\n", ds->m_alias[0]);
    //    std::string alias("aa");
    std::cout<< "-- SHARD ALIAS " << alias << std::endl;
    m_shardmap.insert(std::pair<std::string, dshardctx *>(*alias, ds));
  }

  dshardctx *get_dshard_with_alias(std::string &alias){
    dshardctx *ret = NULL;
    auto p = m_shardmap.find(alias);
    if(p == m_shardmap.end()){
      ret=NULL;
    }else{
      ret = p->second;
    }
    return ret;    
  }

  std::map<std::string, dshardctx *>m_shardmap; // alias and dshardctx mapping
  std::map<int64_t, staleinfo *>m_stale;

  int64_t get_freethrdscnt(void){
    int64_t ret;
    pthread_mutex_lock(&m_freethrds_lock);
    ret = m_freethrds;
    pthread_mutex_unlock(&m_freethrds_lock);
    return ret;
  }

  pthread_mutex_t m_freethrds_lock;
  int64_t m_freethrds;
  scheduler_machctx *m_schedctx;
  coordinator_machctx *m_coordctx;
  worker_machctx *m_workerctx;

  void fork_machagent(void){

    assert(m_mrole != mrole_unknown);
    mach_role role = m_mrole;
    switch(role){
    case mrole_coordinator:

      strads_msg(ERR, " Coordinator call fork agent %d rank  \n", rank);
      m_coordctx = new coordinator_machctx(this);
      break;

    case mrole_scheduler:

      strads_msg(ERR, " Scheduler call fork agent %d rank  \n", rank);
      m_schedctx = new scheduler_machctx(this);
      break;

    case mrole_worker:

      strads_msg(ERR, " Worker call fork agent ... %d rank  \n", rank);
      m_workerctx = new worker_machctx(this);
      break;

    default: 
      assert(0);
    }
  }

  // blocked sync send
  void send(void *buffer, long len){ // workers/schedulers send msg to the coordinator
    assert(m_mrole != mrole_coordinator);       
    star_sendportmap[0]->ctx->push_entry_outq((void *)buffer, len);
  }

  void send(void *buffer, long len, dsttype ws, int wsrank){ // workers/schedulers send msg to the coordinator
    assert(m_mrole == mrole_coordinator); // worker and scheduler are not allowed to use this interface 
    if(ws == dst_scheduler){
      scheduler_sendportmap[wsrank]->ctx->push_entry_outq((void *)buffer, len);
    }else if(ws == dst_worker){
      worker_sendportmap[wsrank]->ctx->push_entry_outq((void *)buffer, len);
    }else{
      assert(0); // unidentified destination type 
    }   
  }

  // asynchronous receiver 
  void *async_recv(void){
    void *ret = NULL;
    assert(m_mrole != mrole_coordinator);       
    ret = star_recvportmap[0]->ctx->pull_entry_inq();
    return ret;
  }
  void *sync_recv(void){
    void *ret = NULL;
    assert(m_mrole != mrole_coordinator);       
    while(ret == NULL){
      ret = star_recvportmap[0]->ctx->pull_entry_inq();
    }
    return ret;
  }


  // asynchronous receiver 
  void *async_recv(int *rlen){
    void *ret = NULL;
    int length=-1;
    assert(m_mrole != mrole_coordinator);       
    ret = star_recvportmap[0]->ctx->pull_entry_inq(&length);
    *rlen = length;
    return ret;
  }
  void *sync_recv(int *rlen){
    int length=-1;
    void *ret = NULL;
    assert(m_mrole != mrole_coordinator);       
    while(ret == NULL){
      ret = star_recvportmap[0]->ctx->pull_entry_inq(&length);     
    }
    *rlen = length;
    return ret;
  }

  void *async_recv(srctype ws, int wsrank){ // workers/schedulers send msg to the coordinator
    void *ret = NULL;
    assert(m_mrole == mrole_coordinator); // worker and scheduler are not allowed to use this interface 
    if(ws == src_scheduler){
      ret = scheduler_recvportmap[wsrank]->ctx->pull_entry_inq();
    }else if(ws == src_worker){
      ret = worker_recvportmap[wsrank]->ctx->pull_entry_inq();
    }else{
      assert(0); // unidentified destination type 
    }   
    return ret;
  }
  void *sync_recv(srctype ws, int wsrank){ // workers/schedulers send msg to the coordinator
    void *ret = NULL;
    assert(m_mrole == mrole_coordinator); // worker and scheduler are not allowed to use this interface 
    if(ws == src_scheduler){
      while(ret == NULL){
	ret = scheduler_recvportmap[wsrank]->ctx->pull_entry_inq();
      }
    }else if(ws == src_worker){
      while(ret == NULL){
	ret = worker_recvportmap[wsrank]->ctx->pull_entry_inq();
      }
    }else{
      assert(0); // unidentified destination type 
    }   
    return ret;
  }

  void *async_recv(srctype ws, int wsrank, int *rlen){ // workers/schedulers send msg to the coordinator

    int length = -1;
    void *ret = NULL;
    assert(m_mrole == mrole_coordinator); // worker and scheduler are not allowed to use this interface 
    if(ws == src_scheduler){
      ret = scheduler_recvportmap[wsrank]->ctx->pull_entry_inq(&length);
    }else if(ws == src_worker){
      ret = worker_recvportmap[wsrank]->ctx->pull_entry_inq();
    }else{
      assert(0); // unidentified destination type 
    }   
    *rlen= length;
    return ret;
  }

  void *sync_recv(srctype ws, int wsrank, int *rlen){ // workers/schedulers send msg to the coordinator
    void *ret = NULL;
    int length=-1;;
    assert(m_mrole == mrole_coordinator); // worker and scheduler are not allowed to use this interface 
    if(ws == src_scheduler){
      while(ret == NULL){
	ret = scheduler_recvportmap[wsrank]->ctx->pull_entry_inq(&length);
      }
    }else if(ws == src_worker){
      while(ret == NULL){
	ret = worker_recvportmap[wsrank]->ctx->pull_entry_inq(&length);
      }
    }else{
      assert(0); // unidentified destination type 
    }   
    *rlen = length;
    return ret;
  }

  pthread_mutex_t m_ringtoken_send_lock;
  pthread_mutex_t m_ringtoken_recv_lock;
  int64_t m_ringtoken_send;
  int64_t m_ringtoken_recv;

  void *ring_asyncrecv_aux(void){
    assert(0);
    void *ret = NULL;
    pthread_mutex_lock(&m_ringtoken_recv_lock);
    assert(m_ringtoken_recv <= IHWM);
    if(m_mrole == mrole_coordinator){
      ret = ring_recvportmap[rdataport]->ctx->ring_pull_entry_inq();
    }else if(m_mrole == mrole_worker){
      ret = ring_recvportmap[rdataport]->ctx->ring_pull_entry_inq();
    }else{
      strads_msg(ERR, "Scheduler is not supposed to user RING API yet\n");
      assert(0);
    }
  
    if(ret != NULL){
      m_ringtoken_recv--;
      assert(m_ringtoken_recv >= 0);
      if(m_ringtoken_recv == 0){
	if(m_mrole == mrole_coordinator){
	  void *tmpbuf = (void *)calloc(1024, sizeof(char));
	  sprintf((char *)tmpbuf, "TOKEN from receiver to sender");
	  ring_sendportmap[rackport]->ctx->ring_push_entry_outq((void *)tmpbuf, 1024);
	  m_ringtoken_recv = IHWM;
	}else if(m_mrole == mrole_worker){
	  void *tmpbuf = (void *)calloc(1024, sizeof(char));
	  sprintf((char *)tmpbuf, "###### TOKEN from receiver (%d) to sender", rank);
	  ring_sendportmap[rackport]->ctx->ring_push_entry_outq((void *)tmpbuf, 1024);
	  m_ringtoken_recv = IHWM;
	}else{
	  strads_msg(ERR, "Scheduler is not supposed to user RING API yet\n");
	  assert(0);
	}
      }      
    }
    pthread_mutex_unlock(&m_ringtoken_recv_lock);
    return ret;
  } // ring_asyncrecv 

  void ring_send_aux(void * buffer, long blen){
    assert(0);

    pthread_mutex_lock(&m_ringtoken_send_lock);
    if(m_ringtoken_send == 0){
      if(m_mrole == mrole_coordinator){
	while(1){
	  void *recv = NULL;
	  while(recv == NULL){
	    recv = ring_recvportmap[rackport]->ctx->ring_pull_entry_inq();
	  }
	  m_ringtoken_send = IHWM;
	  break;
	}
      }else if(m_mrole == mrole_worker){
	while(1){
	  void *recv = NULL;
	  while(recv == NULL){
	    recv = ring_recvportmap[rackport]->ctx->ring_pull_entry_inq();
	  }
	  m_ringtoken_send = IHWM;
	  break;
	}
      }
    }
    if(m_mrole == mrole_coordinator){
      ring_sendportmap[rdataport]->ctx->ring_push_entry_outq((void *)buffer, blen);
    }else if(m_mrole == mrole_worker){
      ring_sendportmap[rdataport]->ctx->ring_push_entry_outq((void *)buffer, blen);
    }else{
      strads_msg(ERR, "Scheduler is not supposed to user RING API yet\n");
      assert(0);
    }
    m_ringtoken_send--;
    assert(m_ringtoken_send >= 0);
    pthread_mutex_unlock(&m_ringtoken_send_lock);
  } // ring_asyncrecv 

  void *ring_async_send_aux(void * buffer, long blen){
    assert(buffer != NULL);
    assert(m_mrole != mrole_scheduler);
    pthread_mutex_lock(&m_ringtoken_send_lock);
    if(m_ringtoken_send == 0){
      while(1){
	void *recv = NULL;      
	recv = ring_recvportmap[rackport]->ctx->ring_pull_entry_inq();	
	if(recv != NULL){
	  m_ringtoken_send = IHWM;	    
	  break;
	}else{
	  pthread_mutex_unlock(&m_ringtoken_send_lock);
	  return NULL;
	}
      }
    }
    ring_sendportmap[rdataport]->ctx->ring_push_entry_outq((void *)buffer, blen);
    m_ringtoken_send--;
    assert(m_ringtoken_send >= 0);
    pthread_mutex_unlock(&m_ringtoken_send_lock);
    return buffer;
  } // ring_asyncrecv 

  void *ring_asyncrecv_aux(int *rlen){
    assert(m_mrole != mrole_scheduler);
    void *ret = NULL;
    pthread_mutex_lock(&m_ringtoken_recv_lock);
    assert(m_ringtoken_recv <= IHWM);
    ret = ring_recvportmap[rdataport]->ctx->ring_pull_entry_inq(rlen);
    if(ret != NULL){
      m_ringtoken_recv++;
      assert(m_ringtoken_recv <= IHWM);
      if(m_ringtoken_recv == IHWM){	
	void *tmpbuf = (void *)calloc(32, sizeof(char));
	sprintf((char *)tmpbuf, "TOKEN");
	ring_sendportmap[rackport]->ctx->ring_push_entry_outq((void *)tmpbuf, 32);
	m_ringtoken_recv = 0;
      }
    }      
    pthread_mutex_unlock(&m_ringtoken_recv_lock);
    return ret;
  } // ring_asyncrecv 

  std::map<int, mnode *> ps_nodes; // machine nodes for ps configuration 
  std::map<int, mlink *> ps_links; // link for ps configuration 

  std::map<int, _ringport *> ps_sendportmap; // ps client/server
  std::map<int, _ringport *> ps_recvportmap; // ps client/server
  
  //  std::map<int, _ringport *> ps_sendportmap; // ps server 
  //  std::map<int, _ringport *> ps_recvportmap; // ps server 

  void (*ps_callback_func)(sharedctx *, std::string &, std::string &); 
  void register_ps_callback_func(void (*func)(sharedctx *, std::string &, std::string &)){
    ps_callback_func = func;
  }
  void (*ps_server_pgasyncfunc)(std::string &, std::string &, sharedctx *); 
  void register_ps_server_pgasyncfunc(void (*func)(std::string &, std::string &, sharedctx *)){
    ps_server_pgasyncfunc = func;
  }
  void (*ps_server_putsyncfunc)(std::string &, std::string &, sharedctx *); 
  void register_ps_server_putsyncfunc(void (*func)(std::string &, std::string &, sharedctx *)){
    ps_server_putsyncfunc = func;
  }
  void (*ps_server_getsyncfunc)(std::string &, std::string &, sharedctx *); 
  void register_ps_server_getsyncfunc(void (*func)(std::string &, std::string &, sharedctx *)){
    ps_server_getsyncfunc = func;
  }

  void *m_tablectx;
  void register_tablectx(void *tablectx){
    m_max_pend = MAX_ZMQ_HWM/(m_worker_machines*2);
    assert(m_max_pend > 10); // increase max zmq hwm 
    strads_msg(ERR, "[**** conf] m_max_pend: %ld \n", m_max_pend);

    m_tablectx = tablectx;
  }

  pthread_cond_t m_upsignal_syncput; 
  pthread_cond_t m_upsignal_syncget; 
  pthread_mutex_t m_lock_syncput;  
  pthread_mutex_t m_lock_syncget; 

  void increment_async_count(void){
    while(!_increment_async_count());
  }

  bool _increment_async_count(void){
    bool ret=false;
    pthread_mutex_lock(&m_lock_async_count);
    if(m_async_count <  m_max_pend){
      m_async_count++;
      ret = true;
    }else{

      strads_msg(ERR, "[worker rank %d ] over the maxpend : %ld -- m_async_count: %ld \n",
		 rank, m_max_pend, m_async_count); 
    }
    pthread_mutex_unlock(&m_lock_async_count);
    return ret;

  }

  void decrement_async_count(void){
    pthread_mutex_lock(&m_lock_async_count);
    assert(m_async_count > 0);
    m_async_count--;
    pthread_mutex_unlock(&m_lock_async_count);
  }

  long m_max_pend;
  long m_async_count;
  pthread_mutex_t m_lock_async_count;    
  sharedctx *__ctx;
  void *sub_comm_ptr;

  void initNetwork(void){

    sysparam *params = m_sp;
    int mpi_size = m_mpi_size;
    int mpi_rank = rank;

    find_role(mpi_size);
    bind_params(params);
    fork_machagent();
    parse_nodefile(params->m_nodefile, *this);
    std::string validip;
    util_find_validip(validip, *this);
    LOG(INFO) << "Rank (" << mpi_rank << ")'s IP found in node file " << validip;

    if((unsigned)mpi_size != nodes.size()){
      LOG(INFO) << "Fatal: mpi_size(" << mpi_size <<") != (" << nodes.size() << ")";
    }

    if(params->m_ps_linkfile.size() > 0 and params->m_ps_nodefile.size() > 0){
      parse_ps_nodefile(params->m_ps_nodefile, *this);
      parse_ps_linkfile(params->m_ps_linkfile, *this);
      strads_msg(ERR, "[*****] ps configuration files(node/link) are parsed\n");
    }else{
      strads_msg(ERR, "No configuration !!!! \n");
    }

    int io_threads= STRADS_ZMQ_IO_THREADS;
    zmq::context_t *contextzmq = new zmq::context_t(io_threads);
    m_zmqcontext = contextzmq;

    strads_msg(ERR, "############################## rank(%d) role(%d) \n", rank, m_mrole);

    if(params->m_topology.compare("star") == 0){
      parse_starlinkfile(params->m_linkfile, *this);
      create_star_ethernet(this, *contextzmq, mpi_size, nodes[mpi_size-1]->ip);
      parse_starlinkfile(params->m_rlinkfile, *this);
      create_ringworker_ethernet_aux(this, *contextzmq, mpi_size, nodes[mpi_size-1]->ip);
      LOG(INFO) << "Star Topology is cretaed with " << mpi_size << " machines (processes) ";
      
    }else{
      LOG(INFO) << "Ring Topology is being created";
      parse_starlinkfile(params->m_linkfile, *this);
      create_ring_ethernet(this, *contextzmq, mpi_size, nodes[mpi_size-1]->ip);
    }


    // set up send / recv port for parameter server / cyclone 
    if(params->m_ps_linkfile.size() > 0 and params->m_ps_nodefile.size() > 0){
      for(int i=0; i < m_sched_machines; i++){
	if(rank == m_first_schedmach + i or m_mrole == mrole_worker ){
	  create_ps_star_ethernet(this, *contextzmq, mpi_size, nodes[m_first_schedmach + i]->ip, m_first_schedmach + i);
	}
      }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    LOG(INFO) <<  " EXIT : in stards_init function MPI RANK :  " << mpi_rank; 
    strads_msg(ERR, "EXIT strads init (%d) role(%d) \n", rank, m_mrole);
  }

}; // sharectx 


