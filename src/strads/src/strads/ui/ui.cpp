//#include <vector> 
//#include <strads/include/common.hpp>
#include <strads/ui/ui.hpp>

void *Strads::sync_recv(srctype s, int id){
  std::lock_guard<std::mutex> lock(apiLock[0]);
  return __ctx->sync_recv(s, id);
}

Strads::Strads(int rank, sysparam *p, int machineCount)
  : sharedctx(rank, p, machineCount), 
    rank(__ctx->rank), 
    schedulerCount(__ctx->m_sched_machines),
    workerCount(__ctx->m_worker_machines){
  __ctx->initNetwork();         // initialize network topology - star, ring, ps if necessary
  machineRole = __ctx->m_mrole; // should be initialized here since initNetwork set m_mrole    

  eCycloneRole cRole = cycloneUnknown;

  if(__ctx->m_mrole == mrole_worker)cRole = cycloneClient ;
  if(__ctx->m_mrole == mrole_scheduler)cRole = cycloneServer;
  //  assert(cRole == cycloneClient or cRole == cycloneServer);  
  int serverCount = __ctx->m_sched_machines;
  int clientCount = __ctx->m_worker_machines;
  if(cRole == cycloneClient or cRole == cycloneServer){
    Cyclone::setCyclone(cRole, serverCount, clientCount, rank, __ctx->ps_sendportmap, __ctx->ps_recvportmap, __ctx->m_max_pend);
    pKV =  Cyclone::getCycloneInstance();
  }else{
    pKV = NULL;
  }

}
