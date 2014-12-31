/********************************************************
  @Authoro: Jin Kyu Kim (jinkyuk@cs.cmu.edu)
  @desc: Warpper functions for the communication primitives 
         It provides common abstractions on top of 
         various communication library - MPI, ZMQ, RDMA

********************************************************/

#include <strads/include/common.hpp>
//#include "ds/dshard.hpp"
#include <strads/netdriver/comm.hpp>

using namespace std;

#if defined(INFINIBAND_SUPPORT)

void com_start_server(sharedctx &shctx, uint16_t port, bool fastring)
{
#if defined(COM_RDMA_PRIMITIVES)
  //  util_setaffinity(RDMA_CPU_AFFINITY);
  LOG_IF(INFO, shctx.rank == 0) << "COM_START_SERVER: " << endl;
  // TODO: for non ring topology, extend this function
  for(auto p : shctx.nodes){
    LOG_IF(INFO, shctx.rank == 0) << "\tNode("<<p.first<<") Rank " << p.second->id << " IP " << p.second->ip 
				  << " func " << p.second->funcname;
  }
  //  context *s_ctx = (context *)calloc(1, sizeof(context));
  //  //  context *s_ctx = new context(100000); 
  //  context *s_ctx = new context(3500); 
  //  context *s_ctx = new context(RDMA_USER_BUFFER_POOL); 



  //  void *memptr = (void *)numa_alloc_onnode(sizeof(context), RDMA_NUMA_MEM_NODE);
  void *memptr = (void *)calloc(1, sizeof(context));
  context *s_ctx = new(memptr)context(RDMA_USER_BUFFER_POOL); 

  //  class _ringport *tmp = new class _ringport;
  //  void *ptr = (void *)numa_alloc_onnode(sizeof(context), RDMA_NUMA_MEM_NODE);
  void *ptr = (void *)calloc(1, sizeof(context));
  class _ringport *tmp = new(ptr) class _ringport;
  tmp->ctx = s_ctx;

  if(fastring != true){ // data ring 
    // TODO: When adding binary tree. expand it. 
    shctx.set_rport(tmp);
    shctx.set_rport_flag(true);
    strads_msg(ERR, "@@@@@@@@@@@@@@@@@ START SERVER Rank(%d) ctx(%p) FOR DATA RING \n", shctx.rank, shctx.ring_rport->ctx);
  }else{ // fast ring stuff
    shctx.set_frport(tmp);
    shctx.set_frport_flag(true);

    strads_msg(ERR, "@@@@@@@@@@@@@@@@@ START SERVER Rank(%d) ctx(%p) FOR FAST RING \n", shctx.rank, shctx.ring_frport->ctx);
  }
  s_ctx->rank = shctx.rank;
  s_ctx->role = RECV_ROLE;
  rdma_server(port, shctx.rank, s_ctx);
#endif
}




// s port: server's port that the client will connect 
// c port: client's port that will be connected to s port on s ip. 
void com_start_client(sharedctx &shctx, uint16_t sport, string &s_ip, bool fastring, uint16_t cport)
{
#if defined(COM_RDMA_PRIMITIVES)
  //  util_setaffinity(RDMA_CPU_AFFINITY);
  strads_msg(ERR, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ COM START CLIENT IS CALLED @@@@@@@@@@@@@@@@@@@@@@\n");
  LOG_IF(INFO, shctx.rank == 0) << "COM_START_CLIENT: " << endl;
  // TODO: for non ring topology, extend this function
  for(auto p : shctx.nodes){
    LOG_IF(INFO, shctx.rank == 0) << "\tNode("<<p.first<<") Rank " << p.second->id << " IP " << p.second->ip 
				  << " func " << p.second->funcname;
  }
  string s_port = to_string(sport);
  //  context *s_ctx = (context *)calloc(1, sizeof(context));
  //  context *s_ctx = new context(3500); 
  //  context *s_ctx = new context(RDMA_USER_BUFFER_POOL); 
  //  void *memptr = (void *)numa_alloc_onnode(sizeof(context), RDMA_NUMA_MEM_NODE);
  void *memptr = (void *)calloc(1, sizeof(context));

  context *s_ctx = new(memptr)context(RDMA_USER_BUFFER_POOL); 

  //  class _ringport *tmp = new class _ringport;

  //  void *ptr = (void *)numa_alloc_onnode(sizeof(context), RDMA_NUMA_MEM_NODE);
  void *ptr = (void *)calloc(1, sizeof(context));
  class _ringport *tmp = new(ptr) class _ringport;
  tmp->ctx = s_ctx;
  if(fastring != true){ // data ring 
    shctx.set_sport(tmp);
    shctx.set_sport_flag(true);
    strads_msg(ERR, "@@@@@@@@@@@@@@@@@ START CLIENT Rank(%d) ctx(%p) FOR DATA RING RING \n", shctx.rank, shctx.ring_sport->ctx);
  }else{ // fast ring 
    shctx.set_fsport(tmp);
    shctx.set_fsport_flag(true);

    strads_msg(ERR, "@@@@@@@@@@@@@@@@@ START CLIENT Rank(%d) ctx(%p) FOR FAST RING \n", shctx.rank, shctx.ring_fsport->ctx);
  }
  s_ctx->rank = shctx.rank;
  s_ctx->role = SEND_ROLE;
  rdma_client(s_ip, s_port, shctx.rank, s_ctx, cport);
#endif
}

void *recv_thread(void *parg){
  targ *arg = (targ *)parg;
  strads_msg(ERR, "Recv Thrd Rank(%d) pschctx(%p) role(%d) \n", arg->pshctx->rank, arg->pshctx, arg->role);
  if(arg->role == server ){
    strads_msg(ERR, "SERVER CALL \n");
    com_start_server(*arg->pshctx, arg->port, arg->fastring);
  }else{
    LOG(FATAL) << "Parameter Error in Recv Thread " << endl;
    assert(0);
  }
  return NULL;
}

void *send_thread(void *parg){
  targ *arg = (targ *)parg;
  strads_msg(ERR, "Send Thrd Rank(%d) pschctx(%p) role(%d) \n", arg->pshctx->rank, arg->pshctx, arg->role);
  if(arg->role == client){   
    sleep(2);
    strads_msg(ERR, "@@@@@@@@@@@@@@@@@@@@@@@ CLIENT CALL \n");
    com_start_client(*arg->pshctx, arg->port, *(arg->ps_ip), arg->fastring, arg->cport);
  }else{    
    LOG(FATAL) << "Parameter Error in Send Thread " << endl;
  }
  return NULL;
}

/////////////////////////////// End of Ring TOPOLOGY ///////////////////







/////////////////////////////// STAR TOPOLOGY ///////////////////

void com_start_star_server(sharedctx &shctx, uint16_t port, bool fastring)
{
#if defined(COM_RDMA_PRIMITIVES)

  strads_msg(ERR, "-- start star server func: insert: Rank(%d) shctx(%p) \n", 
	     shctx.rank, &shctx);

  //  for(auto p : shctx.nodes){
  //    LOG_IF(INFO, shctx.rank == 0) << "\tNode("<<p.first<<") Rank " << p.second->id << " IP " << p.second->ip 
  //				  << " func " << p.second->funcname;
  //  }


  void *memptr = (void *)calloc(1, sizeof(context));
  assert(memptr != NULL);
  context *s_ctx = new(memptr)context(RDMA_USER_BUFFER_POOL); 

  void *ptr = (void *)calloc(1, sizeof(context));

  assert(ptr != NULL);

  class _ringport *tmp = new(ptr) class _ringport;
  tmp->ctx = s_ctx;
  
  shctx.insert_star_recvport(port, tmp); // use my local port number as a key 

  //  shctx.set_rport(tmp);
  //  shctx.set_rport_flag(true);
  strads_msg(ERR, "@@@@@@@@@@@@@@@@@ START SERVER Rank(%d) ctx (%p) FOR Star Topology Center \n", shctx.rank, s_ctx);

  s_ctx->rank = shctx.rank;
  s_ctx->role = RECV_ROLE;
  rdma_server(port, shctx.rank, s_ctx);
#endif
}

// s port: server's port that the client will connect 
// c port: client's port that will be connected to s port on s ip. 
void com_start_star_client(sharedctx &shctx, uint16_t sport, string &s_ip, bool fastring, uint16_t cport)
{
#if defined(COM_RDMA_PRIMITIVES)

  strads_msg(ERR, "-- start star client func\n");
  LOG_IF(INFO, shctx.rank == 0) << "COM_START_CLIENT: " << endl;

  for(auto p : shctx.nodes){
    LOG_IF(INFO, shctx.rank == 0) << "\tNode("<<p.first<<") Rank " << p.second->id << " IP " << p.second->ip 
				  << " func " << p.second->funcname;
  }

  string s_port = to_string(sport);
  //  void *memptr = (void *)numa_alloc_onnode(sizeof(context), RDMA_NUMA_MEM_NODE);
  void *memptr = (void *)calloc(1, sizeof(context));

  context *s_ctx = new(memptr)context(RDMA_USER_BUFFER_POOL); 

  //  class _ringport *tmp = new class _ringport;
  //  void *ptr = (void *)numa_alloc_onnode(sizeof(context), RDMA_NUMA_MEM_NODE);
  void *ptr = (void *)calloc(1, sizeof(context));


  class _ringport *tmp = new(ptr) class _ringport;
  tmp->ctx = s_ctx;

  shctx.insert_star_sendport(cport, tmp); // use my local port number as a key 

  strads_msg(ERR, "@@@@@@@@@@@@@@@@@ START CLIENT Rank(%d) ctx(%p) FOR Star Topology \n", shctx.rank, s_ctx);
  s_ctx->rank = shctx.rank;
  s_ctx->role = SEND_ROLE;
  rdma_client(s_ip, s_port, shctx.rank, s_ctx, cport);
#endif
}

void *recv_star_thread(void *parg){
  targ *arg = (targ *)parg;
  strads_msg(ERR, "Recv Thrd Rank(%d) pschctx(%p) role(%d) \n", arg->pshctx->rank, arg->pshctx, arg->role);
  if(arg->role == server ){
    strads_msg(ERR, "SERVER CALL \n");
    com_start_star_server(*arg->pshctx, arg->port, arg->fastring);
  }else{
    LOG(FATAL) << "Parameter Error in Recv Thread " << endl;
    assert(0);
  }
  return NULL;
}

void *send_star_thread(void *parg){
  targ *arg = (targ *)parg;
  strads_msg(ERR, "Send Thrd Rank(%d) pschctx(%p) role(%d) \n", arg->pshctx->rank, arg->pshctx, arg->role);
  if(arg->role == client){   
    sleep(2);
    strads_msg(ERR, "@@@@@@@@@@@@@@@@@@@@@@@ CLIENT CALL \n");
    com_start_star_client(*arg->pshctx, arg->port, *(arg->ps_ip), arg->fastring, arg->cport);
  }else{    
    LOG(FATAL) << "Parameter Error in Send Thread " << endl;
  }
  return NULL;
}


#endif 
