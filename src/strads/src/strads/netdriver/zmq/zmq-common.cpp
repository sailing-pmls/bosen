#include <strads/include/common.hpp>
#include <strads/netdriver/zmq/zmq-common.hpp>
#include <strads/netdriver/comm.hpp>
#include <mpi.h>
#include <zmq.hpp>
#include <string>

using namespace std;

#define MAX_BUFFER (1024*16)
#define MAX_MACH (128)
#define ITER_TEST (0)

// local functions 
void worker_ringwakeup(sharedctx *pshctx, int dackport, int rank);
void coordinator_ringwakeup(sharedctx *pshctx, int dackport, int rank);
void create_substar_ethernet(sharedctx *pshctx, zmq::context_t &contextzmq, int mpi_size, string &cip);
void worker_ringwakeup_aux(sharedctx *pshctx, int dackport, int rank);
void coordinator_ringwakeup_aux(sharedctx *pshctx, int dackport, int rank);

bool cpps_send (zmq::socket_t &zport, void *msg, int len) {
  bool rc;
  zmq::message_t request(len);
  memcpy((void *)request.data(), msg, len);
  rc = zport.send(request);
  assert(rc);
  return (rc);
}
                                                                                                                     
bool cpps_sendmore (zmq::socket_t &zport, void *msg, int len) {
  bool rc;
  zmq::message_t request(len);
  memcpy((void *)request.data(), msg, len);
  rc = zport.send(request, ZMQ_SNDMORE);
  assert(rc);
  return (rc);
}

int get_id_msg (zmq::socket_t &zport, int rank, int *identity, char *msg, int len) {
  assert(identity != NULL);
  assert(msg != NULL);
  int msglen = -1;
  for(int i=0; i< 2; i++){
    zmq::message_t request;
    zport.recv(&request);
    char *data = (char *)request.data();
    int size = request.size();
    if(i == 0){
      assert(size != 0);
      *identity = *(int *)data;
      assert(size == 4); // rank id has 4 bytes size                                                                                 
    }
    if(i == 1){
      assert(len >= size);
      memcpy(msg, &data[0], size);
      msglen = size;
    }
    int more;
    size_t more_size = sizeof (more);
    zport.getsockopt (ZMQ_RCVMORE, (void *)&more, &more_size);
    if (!more){
      break;      //  Last message part                          
    }
    if(i == 1){
      assert(0); // if i == 1, never reach here                                                   
    }
  }
  return msglen;
}

int get_single_msg(zmq::socket_t &zport, int rank, char *msg, int len) {
  assert(msg != NULL);
  int msglen = -1;
  zmq::message_t request;
  zport.recv(&request);
  char *data = (char *)request.data();
  int size = request.size();
  assert(len >= size);
  memcpy(msg, &data[0], size);
  msglen = size;
  int more;
  size_t more_size = sizeof (more);
  zport.getsockopt (ZMQ_RCVMORE, (void *)&more, &more_size);
  if (more){
    assert(0);
  }
  return msglen;
}

void create_star_ethernet(sharedctx *pshctx, zmq::context_t &contextzmq, int mpi_size, string &cip)
{
  int hwm, hwm_send;
  size_t hwmsize = sizeof(hwm);
  size_t hwm_send_size = sizeof(hwm_send);
  int rank = pshctx->rank;

  char *buffer = (char *)calloc(sizeof(char), MAX_BUFFER); // max message size is limited to 2048 bytes now                       
  int *idcnt_s = (int *)calloc(sizeof(int), MAX_MACH);
  memset((void *)idcnt_s, 0x0, sizeof(int)*MAX_MACH);

  int *idcnt_w = (int *)calloc(sizeof(int), MAX_MACH);
  memset((void *)idcnt_w, 0x0, sizeof(int)*MAX_MACH);

  mach_role mrole = pshctx->find_role(mpi_size);
  char *tmpcstring = (char *)calloc(sizeof(char), 128);

  if(mrole == mrole_coordinator){

    int schedcnt=0;
    int workercnt=0;
    int schedcnt_r=0;
    int workercnt_r=0;   

    for(auto const &p : pshctx->links){
      mlink *tmp = p.second;

      // for receiving port
      if(tmp->dstnode == pshctx->rank){
	int dstport = tmp->dstport; 
	int srcnode = tmp->srcnode;
	zmq::socket_t *pport_s = new zmq::socket_t(contextzmq, ZMQ_ROUTER);
	int sethwm = MAX_ZMQ_HWM;
	pport_s->setsockopt(ZMQ_RCVHWM, &sethwm, sizeof(int)); 
	sprintf(tmpcstring, "tcp://*:%d", dstport); 
	pport_s->bind (tmpcstring); // open 5555 for all incomping connection      
	pport_s->getsockopt(ZMQ_RCVHWM, (void *)&hwm, &hwmsize);
	strads_msg(ERR, "@@@@@@@ Coordinator rank %d open a port%s FOR RECEIVE PORT -- ptr to zmqsocket(%p) HWM(%d)\n", 
		   pshctx->rank, tmpcstring, pport_s, hwm);
	mach_role dstmrole = pshctx->find_role(mpi_size, srcnode);
	_ringport *recvport = new class _ringport;
	recvport->ctx = new context((void*)pport_s, mrole_coordinator);
	if(dstmrole == mrole_scheduler){
	  pshctx->scheduler_recvportmap.insert(pair<int, _ringport*>(schedcnt, recvport));	  
	  schedcnt++;
	}else if(dstmrole == mrole_worker){
	  pshctx->worker_recvportmap.insert(pair<int, _ringport*>(workercnt, recvport));	  
	  workercnt++;
	}else{
	  assert(0);
	}
      }

      // for sending port
      if(tmp->srcnode == pshctx->rank){
	int srcport = tmp->srcport; 
	int dstnode = tmp->dstnode;
	zmq::socket_t *pport_s = new zmq::socket_t(contextzmq, ZMQ_ROUTER);
	int sethwm = MAX_ZMQ_HWM;
	pport_s->setsockopt(ZMQ_SNDHWM, &sethwm, sizeof(int)); 
	sprintf(tmpcstring, "tcp://*:%d", srcport); 
	pport_s->bind (tmpcstring);      
	pport_s->getsockopt(ZMQ_RCVHWM, (void *)&hwm, &hwmsize);
	pport_s->getsockopt(ZMQ_SNDHWM, (void *)&hwm_send, &hwm_send_size);
	strads_msg(ERR, "Coordinator rank %d open a port%s FOR SEND PORT -- ptr to zmqsocket(%p) RCVHWM(%d) SNDHWM(%d)\n", 
		   pshctx->rank, tmpcstring, pport_s, hwm, hwm_send);
	mach_role srcmrole = pshctx->find_role(mpi_size, dstnode);
	_ringport *sendport = new class _ringport;
	sendport->ctx = new context((void*)pport_s, mrole_coordinator);
	if(srcmrole == mrole_scheduler){
	  pshctx->scheduler_sendportmap.insert(pair<int, _ringport*>(schedcnt_r, sendport));	  
	  schedcnt_r++;
	}else if(srcmrole == mrole_worker){
	  pshctx->worker_sendportmap.insert(pair<int, _ringport*>(workercnt_r, sendport));	  
	  workercnt_r++;
	}else{
	  assert(0);
	}
      }
    }

    strads_msg(ERR, "[Coordinator] Open ports and start hands shaking worker_recvportsize(%ld) worker_sendportsize(%ld)\n", 
	       pshctx->worker_recvportmap.size(),
	       pshctx->worker_sendportmap.size());



    for(unsigned int i=0; i<pshctx->worker_recvportmap.size(); i++){
      int id = 1;
      zmq::socket_t *port_r = pshctx->worker_recvportmap[i]->ctx->m_zmqsocket;
      strads_msg(ERR, "Coordinator -- wait for %dth worker RECV out of  %ld  pport(%p)\n", 
		 i, pshctx->worker_sendportmap.size(), port_r);

      get_id_msg(*port_r, rank, &id, buffer, MAX_BUFFER);      
      assert(id == 1);
      cpps_sendmore(*port_r, (void *)&id, 4);
      string msg1("Go");
      cpps_send(*port_r, (void *)msg1.c_str(), MAX_BUFFER-1);   
    }


    strads_msg(ERR, "[Coordinator @@@@@] finish worker recv port confirm \n");


    for(unsigned int i=0; i<pshctx->worker_sendportmap.size(); i++){
      int id = 0;
      zmq::socket_t *port_r = pshctx->worker_sendportmap[i]->ctx->m_zmqsocket;
      strads_msg(ERR, "Coordinator -- wait for %dth worker SENDPORT out of  %ld  pport(%p)\n", 
		 i, pshctx->worker_sendportmap.size(), port_r);
      get_id_msg(*port_r, rank, &id, buffer, MAX_BUFFER);      
      cpps_sendmore(*port_r, (void *)&id, 4);
      string msg1("Go");
      cpps_send(*port_r, (void *)msg1.c_str(), MAX_BUFFER-1);   
    }

    strads_msg(ERR, "[Coordinator @@@@@] finish worker send port confirm \n");


    for(unsigned int i=0; i<pshctx->scheduler_recvportmap.size(); i++){
      int id = 1;
      zmq::socket_t *port_r = pshctx->scheduler_recvportmap[i]->ctx->m_zmqsocket;

      strads_msg(ERR, "Coordinator -- wait for %dth scheduler RECVPORT out of  %ld  pport(%p)\n", 
		 i, pshctx->scheduler_sendportmap.size(), port_r);

      get_id_msg(*port_r, rank, &id, buffer, MAX_BUFFER);      
      assert(id == 1);
      cpps_sendmore(*port_r, (void *)&id, 4);
      string msg1("Go");
      cpps_send(*port_r, (void *)msg1.c_str(), MAX_BUFFER-1);   
    }


    strads_msg(ERR, "[Coordinator @@@@@] finish scheduler recv port confirm \n");
    

    for(unsigned int i=0; i<pshctx->scheduler_sendportmap.size(); i++){
      int id = 0;
      zmq::socket_t *port_r = pshctx->scheduler_sendportmap[i]->ctx->m_zmqsocket;
      strads_msg(ERR, "Coordinator -- wait for %dth scheduler SENDPORT out of  %ld  pport(%p)\n", 
		 i, pshctx->scheduler_sendportmap.size(), port_r);
      get_id_msg(*port_r, rank, &id, buffer, MAX_BUFFER);      
      cpps_sendmore(*port_r, (void *)&id, 4);
      string msg1("Go");
      cpps_send(*port_r, (void *)msg1.c_str(), MAX_BUFFER-1);   
    }
       

    strads_msg(ERR, "[Coordinator @@@@@] finish scheduler send port confirm \n");

  }else if(mrole == mrole_worker or mrole == mrole_scheduler ){

    sleep(2);
    for(auto const &p : pshctx->links){
      mlink *tmp = p.second;

      // for receiving port
      if(tmp->dstnode == pshctx->rank){
	int srcport = tmp->srcport; 
	zmq::socket_t *zport = new zmq::socket_t(contextzmq, ZMQ_DEALER);
	int *identity = (int *)calloc(1, sizeof(int));
	*identity = 1;
	zport->setsockopt(ZMQ_IDENTITY, (void *)identity, sizeof(int));

	int *sethwm = (int *)calloc(1, sizeof(int)); 
	*sethwm = MAX_ZMQ_HWM;
	zport->setsockopt(ZMQ_RCVHWM, sethwm, sizeof(int)); 

	//	sprintf(tmpcstring, "tcp://10.54.1.30:%d", srcport); 
	sprintf(tmpcstring, "tcp://%s:%d", cip.c_str(), srcport); 

	zport->connect (tmpcstring); // open 5555 for all incomping connection      
	zport->getsockopt(ZMQ_RCVHWM, (void *)&hwm, &hwmsize);
	strads_msg(ERR, "RANK rank %d CONNECT a port  %s FOR RECEIVE PORT -- ptr to socket(%p) HWM(%d) \n", 
		   pshctx->rank, tmpcstring, zport, hwm);
	_ringport *recvport = new class _ringport;
	recvport->ctx = new context((void*)zport, mrole);
	pshctx->star_recvportmap.insert(pair<int, _ringport*>(0, recvport));	  
      }

      // for sending port
      if(tmp->srcnode == pshctx->rank){
	int dstport = tmp->dstport; 
	zmq::socket_t *zport = new zmq::socket_t(contextzmq, ZMQ_DEALER);
	int *identity = (int *)calloc(1, sizeof(int));
	*identity = 1;
	zport->setsockopt(ZMQ_IDENTITY, (void *)identity, sizeof(int));

	int sethwm = MAX_ZMQ_HWM;
	zport->setsockopt(ZMQ_SNDHWM, &sethwm, sizeof(int)); 

	//	sprintf(tmpcstring, "tcp://10.54.1.30:%d", dstport); 
	sprintf(tmpcstring, "tcp://%s:%d", cip.c_str(), dstport); 
	zport->connect (tmpcstring); // open 5555 for all incomping connection      
	strads_msg(ERR, "Rank %d CONNECT a port%s FOR SEND PORT -- ptr to socket(%p) \n", 
		   pshctx->rank, tmpcstring, zport);
	_ringport *recvport = new class _ringport;
	recvport->ctx = new context((void*)zport, mrole);
	pshctx->star_sendportmap.insert(pair<int, _ringport*>(0, recvport));	  
      }
    }

    char *buffer = (char *)calloc(sizeof(char), MAX_BUFFER);
    sprintf(buffer, "Heart Beat from rank %d", rank);
    string msg(buffer);
    zmq::socket_t *sendport = pshctx->star_sendportmap[0]->ctx->m_zmqsocket;
    printf("Rank(%d) Send HB to SEND PORT ptr -- socket(%p) \n", rank, sendport);
    cpps_send(*sendport, (void *)msg.c_str(), strlen(msg.c_str()));    
    get_single_msg(*sendport, rank, buffer, MAX_BUFFER);
    strads_msg(OUT, "[Rank %d] got confirm for RECV PORT \n", rank);
    zmq::socket_t *recvport = pshctx->star_recvportmap[0]->ctx->m_zmqsocket;
    printf("Rank(%d) Send HB to RECV PORT ptr -- socket(%p) \n", rank, recvport);
    cpps_send(*recvport, (void *)msg.c_str(), strlen(msg.c_str()));    
    get_single_msg(*recvport, rank, buffer, MAX_BUFFER);
    strads_msg(OUT, "[Rank %d] got confirm for SEND PORT \n", rank);

  }else{
    strads_msg(ERR, "[Fatal: rank %d] MACHINE TYPE IS NOT ASSIGNED YET \n", rank);
    assert(0);
  }

  strads_msg(ERR, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ finish create starring ethernet by rank (%d)\n", rank);
  MPI_Barrier(MPI_COMM_WORLD); 
}

#define RECVPORT_ZMQ (36000)
#define SENDPORT_ZMQ (42000)

void create_star_ethernet_singlemach(sharedctx *pshctx, zmq::context_t &contextzmq, int mpi_size)
{
  int hwm, hwm_send;
  size_t hwmsize = sizeof(hwm);
  size_t hwm_send_size = sizeof(hwm_send);
  int rank = pshctx->rank;

  char *buffer = (char *)calloc(sizeof(char), MAX_BUFFER); // max message size is limited to 2048 bytes now                       
  int *idcnt_s = (int *)calloc(sizeof(int), MAX_MACH);
  memset((void *)idcnt_s, 0x0, sizeof(int)*MAX_MACH);

  int *idcnt_w = (int *)calloc(sizeof(int), MAX_MACH);
  memset((void *)idcnt_w, 0x0, sizeof(int)*MAX_MACH);

  mach_role mrole = pshctx->find_role(mpi_size);
  char *tmpcstring = (char *)calloc(sizeof(char), 128);

  if(mrole == mrole_coordinator){

    int schedcnt=0;
    int workercnt=0;
    int schedcnt_r=0;
    int workercnt_r=0;   


    for(int i=0; i<mpi_size-1; i++){
      //    for(auto const &p : pshctx->links){
      //      mlink *tmp = p.second;
      // for receiving port
      //      if(tmp->dstnode == pshctx->rank){
      int dstport = RECVPORT_ZMQ + i; 
      zmq::socket_t *pport_s = new zmq::socket_t(contextzmq, ZMQ_ROUTER);
      int sethwm = MAX_ZMQ_HWM;
      pport_s->setsockopt(ZMQ_RCVHWM, &sethwm, sizeof(int)); 
      sprintf(tmpcstring, "tcp://*:%d", dstport); 
      pport_s->bind (tmpcstring); // open 5555 for all incomping connection      
      pport_s->getsockopt(ZMQ_RCVHWM, (void *)&hwm, &hwmsize);
      strads_msg(ERR, "@@@@@@@ Coordinator rank %d open a port%s FOR RECEIVE PORT -- ptr to zmqsocket(%p) HWM(%d)\n", 
		 pshctx->rank, tmpcstring, pport_s, hwm);

      mach_role dstmrole;
      if(i==0){
	dstmrole = mrole_worker;
      }else if(i == 1){
	dstmrole = mrole_scheduler;
      }else{
	assert(0);
      }
      //      mach_role dstmrole = pshctx->find_role(mpi_size, srcnode);

      _ringport *recvport = new class _ringport;
      recvport->ctx = new context((void*)pport_s, mrole_coordinator);
      if(dstmrole == mrole_scheduler){
	pshctx->scheduler_recvportmap.insert(pair<int, _ringport*>(schedcnt, recvport));	  
	schedcnt++;
      }else if(dstmrole == mrole_worker){
	pshctx->worker_recvportmap.insert(pair<int, _ringport*>(workercnt, recvport));	  
	workercnt++;
      }else{
	assert(0);
      }


      // for sending port
      int srcport = SENDPORT_ZMQ + i; 
      //      int dstnode = tmp->dstnode;
      pport_s = new zmq::socket_t(contextzmq, ZMQ_ROUTER);
      sethwm = MAX_ZMQ_HWM;
      pport_s->setsockopt(ZMQ_SNDHWM, &sethwm, sizeof(int)); 
      sprintf(tmpcstring, "tcp://*:%d", srcport); 
      pport_s->bind (tmpcstring);      
      pport_s->getsockopt(ZMQ_RCVHWM, (void *)&hwm, &hwmsize);
      pport_s->getsockopt(ZMQ_SNDHWM, (void *)&hwm_send, &hwm_send_size);
      strads_msg(ERR, "Coordinator rank %d open a port%s FOR SEND PORT -- ptr to zmqsocket(%p) RCVHWM(%d) SNDHWM(%d)\n", 
		 pshctx->rank, tmpcstring, pport_s, hwm, hwm_send);

      mach_role srcmrole;
      //      mach_role dstmrole = pshctx->find_role(mpi_size, srcnode);
      if(i==0){
	srcmrole = mrole_worker;
      }else if(i == 1){
	srcmrole = mrole_scheduler;
      }else{
	assert(0);
      }

      _ringport *sendport = new class _ringport;
      sendport->ctx = new context((void*)pport_s, mrole_coordinator);
      if(srcmrole == mrole_scheduler){
	pshctx->scheduler_sendportmap.insert(pair<int, _ringport*>(schedcnt_r, sendport));	  
	schedcnt_r++;
      }else if(srcmrole == mrole_worker){
	pshctx->worker_sendportmap.insert(pair<int, _ringport*>(workercnt_r, sendport));	  
	workercnt_r++;
      }else{
	assert(0);
      }
      
    }


    strads_msg(OUT, "[Coordinator] Open ports and start hands shaking\n");

    for(unsigned int i=0; i<pshctx->worker_recvportmap.size(); i++){
      int id = 1;
      zmq::socket_t *port_r = pshctx->worker_recvportmap[i]->ctx->m_zmqsocket;

      strads_msg(ERR, "Coordinator -- wait for %dth worker RECV out of  %ld  pport(%p)\n", 
		 i, pshctx->worker_sendportmap.size(), port_r);

      get_id_msg(*port_r, rank, &id, buffer, MAX_BUFFER);      
      assert(id == 1);
      cpps_sendmore(*port_r, (void *)&id, 4);
      string msg1("Go");
      cpps_send(*port_r, (void *)msg1.c_str(), MAX_BUFFER-1);   
    }
    
    for(unsigned int i=0; i<pshctx->worker_sendportmap.size(); i++){
      int id = 0;
      zmq::socket_t *port_r = pshctx->worker_sendportmap[i]->ctx->m_zmqsocket;
      strads_msg(ERR, "Coordinator -- wait for %dth worker SENDPORT out of  %ld  pport(%p)\n", 
		 i, pshctx->worker_sendportmap.size(), port_r);
      get_id_msg(*port_r, rank, &id, buffer, MAX_BUFFER);      
      cpps_sendmore(*port_r, (void *)&id, 4);
      string msg1("Go");
      cpps_send(*port_r, (void *)msg1.c_str(), MAX_BUFFER-1);   
    }

    for(unsigned int i=0; i<pshctx->scheduler_recvportmap.size(); i++){
      int id = 1;
      zmq::socket_t *port_r = pshctx->scheduler_recvportmap[i]->ctx->m_zmqsocket;
      get_id_msg(*port_r, rank, &id, buffer, MAX_BUFFER);      
      assert(id == 1);
      cpps_sendmore(*port_r, (void *)&id, 4);
      string msg1("Go");
      cpps_send(*port_r, (void *)msg1.c_str(), MAX_BUFFER-1);   
    }
    
    for(unsigned int i=0; i<pshctx->scheduler_sendportmap.size(); i++){
      int id = 0;
      zmq::socket_t *port_r = pshctx->scheduler_sendportmap[i]->ctx->m_zmqsocket;
      strads_msg(ERR, "Coordinator -- wait for %dth worker SENDPORT out of  %ld  pport(%p)\n", 
		 i, pshctx->scheduler_sendportmap.size(), port_r);
      get_id_msg(*port_r, rank, &id, buffer, MAX_BUFFER);      
      cpps_sendmore(*port_r, (void *)&id, 4);
      string msg1("Go");
      cpps_send(*port_r, (void *)msg1.c_str(), MAX_BUFFER-1);   
    }
       
  }else if(mrole == mrole_worker or mrole == mrole_scheduler ){

    int srcport = SENDPORT_ZMQ + pshctx->rank; 
    zmq::socket_t *zport = new zmq::socket_t(contextzmq, ZMQ_DEALER);
    int *identity = (int *)calloc(1, sizeof(int));
    *identity = 1;
    zport->setsockopt(ZMQ_IDENTITY, (void *)identity, sizeof(int));
    int *sethwm = (int *)calloc(1, sizeof(int)); 
    *sethwm = MAX_ZMQ_HWM;
    zport->setsockopt(ZMQ_RCVHWM, sethwm, sizeof(int)); 
    //    sprintf(tmpcstring, "tcp://%s:%d", "localhost", srcport); 
    sprintf(tmpcstring, "tcp://localhost:%d", srcport); 
    zport->connect (tmpcstring); // open 5555 for all incomping connection      
    zport->getsockopt(ZMQ_RCVHWM, (void *)&hwm, &hwmsize);
    strads_msg(ERR, "RANK rank %d CONNECT a port  %s FOR RECEIVE PORT -- ptr to socket(%p) HWM(%d) \n", 
	       pshctx->rank, tmpcstring, zport, hwm);
    _ringport *recvport = new class _ringport;
    recvport->ctx = new context((void*)zport, mrole);
    pshctx->star_recvportmap.insert(pair<int, _ringport*>(0, recvport));	  


    int dstport = RECVPORT_ZMQ + pshctx->rank;
    zport = new zmq::socket_t(contextzmq, ZMQ_DEALER);
    identity = (int *)calloc(1, sizeof(int));
    *identity = 1;
    zport->setsockopt(ZMQ_IDENTITY, (void *)identity, sizeof(int));

    sethwm = (int *)calloc(1, sizeof(int)); 
    *sethwm = MAX_ZMQ_HWM;
    zport->setsockopt(ZMQ_SNDHWM, sethwm, sizeof(int)); 
    //    sprintf(tmpcstring, "tcp://%s:%d", "localhost", dstport); 
    sprintf(tmpcstring, "tcp://localhost:%d", dstport); 
    zport->connect (tmpcstring); // open 5555 for all incomping connection      
    strads_msg(ERR, "Rank %d CONNECT a port%s FOR SEND PORT -- ptr to socket(%p) \n", 
	       pshctx->rank, tmpcstring, zport);
    recvport = new class _ringport;
    recvport->ctx = new context((void*)zport, mrole);
    pshctx->star_sendportmap.insert(pair<int, _ringport*>(0, recvport));	  


    char *buffer = (char *)calloc(sizeof(char), MAX_BUFFER);
    sprintf(buffer, "Heart Beat from rank %d", rank);
    string msg(buffer);
    zmq::socket_t *sendport = pshctx->star_sendportmap[0]->ctx->m_zmqsocket;
    printf("Rank(%d) Send HB to SEND PORT ptr -- socket(%p) \n", rank, sendport);
    cpps_send(*sendport, (void *)msg.c_str(), strlen(msg.c_str()));    
    get_single_msg(*sendport, rank, buffer, MAX_BUFFER);
    strads_msg(OUT, "[Rank %d] got confirm for RECV PORT \n", rank);

    zmq::socket_t *zmqrecvport = pshctx->star_recvportmap[0]->ctx->m_zmqsocket;
    printf("Rank(%d) Send HB to RECV PORT ptr -- socket(%p) \n", rank, zmqrecvport);
    cpps_send(*zmqrecvport, (void *)msg.c_str(), strlen(msg.c_str()));    
    get_single_msg(*zmqrecvport, rank, buffer, MAX_BUFFER);
    strads_msg(OUT, "[Rank %d] got confirm for SEND PORT \n", rank);

  }else{
    strads_msg(ERR, "[Fatal: rank %d] MACHINE TYPE IS NOT ASSIGNED YET \n", rank);
    assert(0);
  }
  //  MPI_Barrier(MPI_COMM_WORLD); 
}

void create_ring_ethernet(sharedctx *pshctx, zmq::context_t &contextzmq, int mpi_size, string &cip)
{
  create_substar_ethernet(pshctx, contextzmq,  mpi_size, cip);
  create_ringworker_ethernet(pshctx, contextzmq,  mpi_size, cip);
  LOG(INFO) << "@@@@@@@@@@@@@@ Ring Created : rank " << pshctx->rank << endl;
}


//////////////////////// CREATE scheduler_coordinator_small_star along with ring 
// for multi machines and single machine configuration. 
// create_substar_ethernet : create a small star between coordinator and schedulers 
// /////////////////////////////////////////////////////////////////////////////////
void create_substar_ethernet(sharedctx *pshctx, zmq::context_t &contextzmq, int mpi_size, string &cip)
{
  int hwm, hwm_send;
  size_t hwmsize = sizeof(hwm);
  size_t hwm_send_size = sizeof(hwm_send);
  int rank = pshctx->rank;

  char *buffer = (char *)calloc(sizeof(char), MAX_BUFFER); // max message size is limited to 2048 bytes now                       
  int *idcnt_s = (int *)calloc(sizeof(int), MAX_MACH);
  memset((void *)idcnt_s, 0x0, sizeof(int)*MAX_MACH);

  int *idcnt_w = (int *)calloc(sizeof(int), MAX_MACH);
  memset((void *)idcnt_w, 0x0, sizeof(int)*MAX_MACH);

  mach_role mrole = pshctx->find_role(mpi_size);
  char *tmpcstring = (char *)calloc(sizeof(char), 128);

  //  int firstsched = pshctx->m_first_schedmach;
  int firstworker = pshctx->m_first_workermach;
  //  int schedmach = pshctx->m_sched_machines;
  int workermach = pshctx->m_worker_machines;;

  if(mrole == mrole_coordinator){

    int schedcnt=0;
    //    int workercnt=0;
    int schedcnt_r=0;
    int workercnt_r=0;   

    for(auto const &p : pshctx->links){
      mlink *tmp = p.second;

      if( (p.second->srcnode >= firstworker and p.second->srcnode <firstworker+workermach) or // if workers related link, skip 
	  (p.second->dstnode >= firstworker and p.second->dstnode <firstworker+workermach)){
	continue;
      }

      // for receiving port
      if(tmp->dstnode == pshctx->rank){

	int dstport = tmp->dstport; 
	int srcnode = tmp->srcnode;
	zmq::socket_t *pport_s = new zmq::socket_t(contextzmq, ZMQ_ROUTER);
	int sethwm = MAX_ZMQ_HWM;
	pport_s->setsockopt(ZMQ_RCVHWM, &sethwm, sizeof(int)); 
	sprintf(tmpcstring, "tcp://*:%d", dstport); 
	pport_s->bind (tmpcstring); // open 5555 for all incomping connection      
	pport_s->getsockopt(ZMQ_RCVHWM, (void *)&hwm, &hwmsize);
	strads_msg(ERR, "[ Ring substar ] Coordinator rank %d open a port%s FOR RECEIVE PORT -- ptr to zmqsocket(%p) HWM(%d)\n", 
		   pshctx->rank, tmpcstring, pport_s, hwm);
	mach_role dstmrole = pshctx->find_role(mpi_size, srcnode);
	_ringport *recvport = new class _ringport;
	recvport->ctx = new context((void*)pport_s, mrole_coordinator);
	if(dstmrole == mrole_scheduler){
	  pshctx->scheduler_recvportmap.insert(pair<int, _ringport*>(schedcnt, recvport));	  
	  schedcnt++;
	}else if(dstmrole == mrole_worker){
	  // do nothing 
	}else{
	  assert(0);
	}
      }

      // for sending port
      if(tmp->srcnode == pshctx->rank){
	int srcport = tmp->srcport; 
	int dstnode = tmp->dstnode;
	zmq::socket_t *pport_s = new zmq::socket_t(contextzmq, ZMQ_ROUTER);
	int sethwm = MAX_ZMQ_HWM;
	pport_s->setsockopt(ZMQ_SNDHWM, &sethwm, sizeof(int)); 
	sprintf(tmpcstring, "tcp://*:%d", srcport); 
	pport_s->bind (tmpcstring);      
	pport_s->getsockopt(ZMQ_RCVHWM, (void *)&hwm, &hwmsize);
	pport_s->getsockopt(ZMQ_SNDHWM, (void *)&hwm_send, &hwm_send_size);
	strads_msg(ERR, "[ Ring substar ] Coordinator rank %d open a port%s FOR SEND PORT -- ptr zmqsocket(%p) RCVHWM(%d) SNDHWM(%d)\n", 
		   pshctx->rank, tmpcstring, pport_s, hwm, hwm_send);
	mach_role srcmrole = pshctx->find_role(mpi_size, dstnode);
	_ringport *sendport = new class _ringport;
	sendport->ctx = new context((void*)pport_s, mrole_coordinator);
	if(srcmrole == mrole_scheduler){
	  pshctx->scheduler_sendportmap.insert(pair<int, _ringport*>(schedcnt_r, sendport));	  
	  schedcnt_r++;
	}else if(srcmrole == mrole_worker){
	  pshctx->worker_sendportmap.insert(pair<int, _ringport*>(workercnt_r, sendport));	  
	  workercnt_r++;
	}else{
	  assert(0);
	}
      }
    }

    for(unsigned int i=0; i<pshctx->scheduler_recvportmap.size(); i++){
      int id = 1;
      zmq::socket_t *port_r = pshctx->scheduler_recvportmap[i]->ctx->m_zmqsocket;

      strads_msg(ERR, "[ Ring substar ] Coordinator -- wait for %dth scheduler RECVPORT out of  %ld  pport(%p)\n", 
		 i, pshctx->scheduler_sendportmap.size(), port_r);

      get_id_msg(*port_r, rank, &id, buffer, MAX_BUFFER);      
      assert(id == 1);
      cpps_sendmore(*port_r, (void *)&id, 4);
      string msg1("Go");
      cpps_send(*port_r, (void *)msg1.c_str(), MAX_BUFFER-1);   
    }


    strads_msg(ERR, "[ Ring substar ], [Coordinator @@@@@] finish scheduler recv port confirm \n");   
    for(unsigned int i=0; i<pshctx->scheduler_sendportmap.size(); i++){
      int id = 0;
      zmq::socket_t *port_r = pshctx->scheduler_sendportmap[i]->ctx->m_zmqsocket;
      strads_msg(ERR, "Coordinator -- wait for %dth scheduler SENDPORT out of  %ld  pport(%p)\n", 
		 i, pshctx->scheduler_sendportmap.size(), port_r);
      get_id_msg(*port_r, rank, &id, buffer, MAX_BUFFER);      
      cpps_sendmore(*port_r, (void *)&id, 4);
      string msg1("Go");
      cpps_send(*port_r, (void *)msg1.c_str(), MAX_BUFFER-1);   
    }
       

    strads_msg(ERR, "[ Ring substar ][Coordinator @@@@@] finish scheduler send port confirm \n");

  }else if(mrole == mrole_scheduler ){

    sleep(2);
    for(auto const &p : pshctx->links){
      mlink *tmp = p.second;

      // for receiving port
      if(tmp->dstnode == pshctx->rank){
	int srcport = tmp->srcport; 
	zmq::socket_t *zport = new zmq::socket_t(contextzmq, ZMQ_DEALER);
	int *identity = (int *)calloc(1, sizeof(int));
	*identity = 1;
	zport->setsockopt(ZMQ_IDENTITY, (void *)identity, sizeof(int));

	int *sethwm = (int *)calloc(1, sizeof(int)); 
	*sethwm = MAX_ZMQ_HWM;
	zport->setsockopt(ZMQ_RCVHWM, sethwm, sizeof(int)); 

	//	sprintf(tmpcstring, "tcp://10.54.1.30:%d", srcport); 
	sprintf(tmpcstring, "tcp://%s:%d", cip.c_str(), srcport); 

	zport->connect (tmpcstring); // open 5555 for all incomping connection      
	zport->getsockopt(ZMQ_RCVHWM, (void *)&hwm, &hwmsize);
	strads_msg(ERR, "[ Ring substar ] RANK rank %d CONNECT a port  %s FOR RECEIVE PORT -- ptr to socket(%p) HWM(%d) \n", 
		   pshctx->rank, tmpcstring, zport, hwm);
	_ringport *recvport = new class _ringport;
	recvport->ctx = new context((void*)zport, mrole);
	pshctx->star_recvportmap.insert(pair<int, _ringport*>(0, recvport));	  
      }

      // for sending port
      if(tmp->srcnode == pshctx->rank){
	int dstport = tmp->dstport; 
	zmq::socket_t *zport = new zmq::socket_t(contextzmq, ZMQ_DEALER);
	int *identity = (int *)calloc(1, sizeof(int));
	*identity = 1;
	zport->setsockopt(ZMQ_IDENTITY, (void *)identity, sizeof(int));

	int sethwm = MAX_ZMQ_HWM;
	zport->setsockopt(ZMQ_SNDHWM, &sethwm, sizeof(int)); 

	//	sprintf(tmpcstring, "tcp://10.54.1.30:%d", dstport); 
	sprintf(tmpcstring, "tcp://%s:%d", cip.c_str(), dstport); 
	zport->connect (tmpcstring); // open 5555 for all incomping connection      
	strads_msg(ERR, "[ Ring substar ] Rank %d CONNECT a port%s FOR SEND PORT -- ptr to socket(%p) \n", 
		   pshctx->rank, tmpcstring, zport);
	_ringport *recvport = new class _ringport;
	recvport->ctx = new context((void*)zport, mrole);
	pshctx->star_sendportmap.insert(pair<int, _ringport*>(0, recvport));	  
      }
    }

    char *buffer = (char *)calloc(sizeof(char), MAX_BUFFER);
    sprintf(buffer, "Heart Beat from rank %d", rank);
    string msg(buffer);
    zmq::socket_t *sendport = pshctx->star_sendportmap[0]->ctx->m_zmqsocket;
    printf("[ Ring substar ] Rank(%d) Send HB to SEND PORT ptr -- socket(%p) \n", rank, sendport);
    cpps_send(*sendport, (void *)msg.c_str(), strlen(msg.c_str()));    
    get_single_msg(*sendport, rank, buffer, MAX_BUFFER);
    strads_msg(OUT, "[Rank %d] got confirm for RECV PORT \n", rank);
    zmq::socket_t *recvport = pshctx->star_recvportmap[0]->ctx->m_zmqsocket;
    printf("[ Ring substar ] Rank(%d) Send HB to RECV PORT ptr -- socket(%p) \n", rank, recvport);
    cpps_send(*recvport, (void *)msg.c_str(), strlen(msg.c_str()));    
    get_single_msg(*recvport, rank, buffer, MAX_BUFFER);
    strads_msg(OUT, "[ Ring substar ] [Rank %d] got confirm for SEND PORT \n", rank);

  }else if(mrole == mrole_worker){

    // do nothing. 
  }else{
    strads_msg(ERR, "[ Ring substar ] [Fatal: rank %d] MACHINE TYPE IS NOT ASSIGNED YET \n", rank);
    assert(0);
  }

  strads_msg(ERR, " [ Ring substar ] finish create starring ethernet by rank (%d)\n", rank);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// create_ringworker with coordinator.
// create a ring on workers and a coordinator nodes 
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void create_ringworker_ethernet(sharedctx *pshctx, zmq::context_t &contextzmq, int mpi_size, string &cip)
{

  strads_msg(ERR, "\n\n\n[ Ring worker ring ] start\n");

  int hwm;
  size_t hwmsize = sizeof(hwm);
  int rank = pshctx->rank;
  int *idcnt_s = (int *)calloc(sizeof(int), MAX_MACH);
  memset((void *)idcnt_s, 0x0, sizeof(int)*MAX_MACH);
  int *idcnt_w = (int *)calloc(sizeof(int), MAX_MACH);
  memset((void *)idcnt_w, 0x0, sizeof(int)*MAX_MACH);
  mach_role mrole = pshctx->find_role(mpi_size);
  char *tmpcstring = (char *)calloc(sizeof(char), 128);
  int firstsched = pshctx->m_first_schedmach;
  //int firstworker = pshctx->m_first_workermach;
  int schedmach = pshctx->m_sched_machines;
  int workermach = pshctx->m_worker_machines;;
  int firstcoord = pshctx->m_first_coordinatormach; 

  assert(firstcoord == (mpi_size -1));

  if(mrole == mrole_coordinator){

    for(auto const &p : pshctx->links){
      mlink *tmp = p.second;

      if( (p.second->srcnode >= firstsched and p.second->srcnode <firstsched+schedmach) or // if workers related link, skip 
	  (p.second->dstnode >= firstsched and p.second->dstnode <firstsched+schedmach)){
	continue;
      }

      // for receiving port
      if(tmp->dstnode == pshctx->rank){
	int dstport = tmp->dstport; 
	int srcnode = tmp->srcnode;
	zmq::socket_t *pport_s = new zmq::socket_t(contextzmq, ZMQ_ROUTER);
	int sethwm = MAX_ZMQ_HWM;
	pport_s->setsockopt(ZMQ_RCVHWM, &sethwm, sizeof(int)); 
	sprintf(tmpcstring, "tcp://*:%d", dstport); 
	pport_s->bind (tmpcstring); // open 5555 for all incomping connection      
	pport_s->getsockopt(ZMQ_RCVHWM, (void *)&hwm, &hwmsize);
	strads_msg(ERR, "@@@@@@@ Coordinator rank %d open a port%s FOR RECEIVE PORT -- ptr to zmqsocket(%p) HWM(%d)\n", 
		   pshctx->rank, tmpcstring, pport_s, hwm);
	mach_role dstmrole = pshctx->find_role(mpi_size, srcnode);
	_ringport *recvport = new class _ringport;
	recvport->ctx = new context((void*)pport_s, mrole_coordinator);
	if(dstmrole == mrole_scheduler){
	  assert(0);
	}else if(dstmrole == mrole_worker){
	  if(srcnode == (workermach-1)){
	    assert(pshctx->worker_recvportmap.size() < 2);
	    pshctx->worker_recvportmap.insert(pair<int, _ringport*>(rdataport, recvport));	  	    
	  }else if(srcnode == 0){
	    assert(pshctx->worker_recvportmap.size() < 2);
	    pshctx->worker_recvportmap.insert(pair<int, _ringport*>(rackport, recvport));	  	    
	  }else{
	    assert(0);
	  }
	}else{
	  assert(0);
	}
      }

      // for sending port
      if(tmp->srcnode == pshctx->rank){
	int dstnode = tmp->dstnode;
	int dstport = tmp->dstport; 
	zmq::socket_t *zport = new zmq::socket_t(contextzmq, ZMQ_DEALER);
	int *identity = (int *)calloc(1, sizeof(int));
	*identity = 1;
	zport->setsockopt(ZMQ_IDENTITY, (void *)identity, sizeof(int));
	int sethwm = MAX_ZMQ_HWM;
	zport->setsockopt(ZMQ_SNDHWM, &sethwm, sizeof(int));
	sprintf(tmpcstring, "tcp://%s:%d", pshctx->nodes[dstnode]->ip.c_str(), dstport); 
	zport->connect (tmpcstring); // open 5555 for all incomping connection      
	strads_msg(ERR, "Rank %d CONNECT a port%s FOR SEND PORT -- ptr to socket(%p) \n", 
		   pshctx->rank, tmpcstring, zport);
	mach_role srcmrole = pshctx->find_role(mpi_size, dstnode);
	_ringport *sendport = new class _ringport;
	sendport->ctx = new context((void*)zport, mrole_coordinator);
	if(srcmrole == mrole_scheduler){
	  assert(0);
	}else if(srcmrole == mrole_worker){
	  if(dstnode == (workermach-1)){
	    assert(pshctx->worker_sendportmap.size() < 2);
	    pshctx->worker_sendportmap.insert(pair<int, _ringport*>(rackport, sendport));	  
	  }else if(dstnode == 0){
	    assert(pshctx->worker_sendportmap.size() < 2);
	    strads_msg(ERR, "coordinator put rdataport into sendport for dstnode %d (sendport[%p]\n", dstnode, sendport->ctx);
	    pshctx->worker_sendportmap.insert(pair<int, _ringport*>(rdataport, sendport));	  
	  }else{
	    assert(0);
	  }
	}else{
	  assert(0);
	}

      }
    }

    assert(pshctx->worker_sendportmap.size() == 2);
    assert(pshctx->worker_recvportmap.size() == 2);
    coordinator_ringwakeup(pshctx, rdataport, rank);
    coordinator_ringwakeup(pshctx, rackport, rank);
  }else if(mrole == mrole_worker ){
    //    int rcnt = 0;
    //    int scnt = 0;
    sleep(2);
    for(auto const &p : pshctx->links){
      mlink *tmp = p.second;
      if( (p.second->srcnode >= firstsched and p.second->srcnode <firstsched+schedmach) or // if workers related link, skip 
	  (p.second->dstnode >= firstsched and p.second->dstnode <firstsched+schedmach)){
	continue;
      }
      // for receiving port
      if(tmp->dstnode == pshctx->rank){
	int dstport = tmp->dstport; 
	int srcnode = tmp->srcnode;
	zmq::socket_t *pport_s = new zmq::socket_t(contextzmq, ZMQ_ROUTER);
	int sethwm = MAX_ZMQ_HWM;
	pport_s->setsockopt(ZMQ_RCVHWM, &sethwm, sizeof(int)); 
	sprintf(tmpcstring, "tcp://*:%d", dstport); 
	pport_s->bind (tmpcstring); // open 5555 for all incomping connection      
	pport_s->getsockopt(ZMQ_RCVHWM, (void *)&hwm, &hwmsize);
	strads_msg(ERR, "@@@@@@@ WORKER rank %d open a port%s FOR RECEIVE PORT -- ptr to zmqsocket(%p) HWM(%d)\n", 
		   pshctx->rank, tmpcstring, pport_s, hwm);
	//mach_role dstmrole = pshctx->find_role(mpi_size, srcnode);
	_ringport *recvport = new class _ringport;
	recvport->ctx = new context((void*)pport_s, mrole_coordinator);
	if(pshctx->rank == 0){ // the first worker 
	  if(srcnode == firstcoord){
	    assert(pshctx->star_recvportmap.size() < 2);
	    pshctx->star_recvportmap.insert(pair<int, _ringport*>(rdataport, recvport)); // for data port 	  
	  }else{
	    assert(pshctx->star_recvportmap.size() < 2);
	    pshctx->star_recvportmap.insert(pair<int, _ringport*>(rackport, recvport)); // for data port 	  
	  }
	}else if(pshctx->rank == (workermach-1)){ // the last worker 
	  if(srcnode == firstcoord){
	    assert(pshctx->star_recvportmap.size() < 2);
	    pshctx->star_recvportmap.insert(pair<int, _ringport*>(rackport, recvport)); // for data port 	  
	  }else{
	    assert(pshctx->star_recvportmap.size() < 2);
	    assert(srcnode == (pshctx->rank - 1));
	    pshctx->star_recvportmap.insert(pair<int, _ringport*>(rdataport, recvport)); // for data port 	  
	  }
	}else{ // workers between the first and last workers. 
	  int rank = pshctx->rank;
	  if(srcnode == (rank -1) ){
	    assert(pshctx->star_recvportmap.size() < 2);
	    pshctx->star_recvportmap.insert(pair<int, _ringport*>(rdataport, recvport)); // for data port 	  
	  }else{
	    assert(pshctx->star_recvportmap.size() < 2);
	    pshctx->star_recvportmap.insert(pair<int, _ringport*>(rackport, recvport)); // for data port 	  
	  }
	}
      }

      // for sending port
      if(tmp->srcnode == pshctx->rank){
	int dstnode = tmp->dstnode;
	int dstport = tmp->dstport; 
	zmq::socket_t *zport = new zmq::socket_t(contextzmq, ZMQ_DEALER);
	int *identity = (int *)calloc(1, sizeof(int));
	*identity = 1;
	zport->setsockopt(ZMQ_IDENTITY, (void *)identity, sizeof(int));
	int sethwm = MAX_ZMQ_HWM;
	zport->setsockopt(ZMQ_SNDHWM, &sethwm, sizeof(int));

	//sprintf(tmpcstring, "tcp://%s:%d", cip.c_str(), dstport); 
	sprintf(tmpcstring, "tcp://%s:%d", pshctx->nodes[dstnode]->ip.c_str(), dstport); 
	zport->connect (tmpcstring); // open 5555 for all incomping connection      
	strads_msg(ERR, "Rank %d CONNECT a port%s FOR SEND PORT -- ptr to socket(%p) \n", 
		   pshctx->rank, tmpcstring, zport);
	_ringport *recvport = new class _ringport;
	recvport->ctx = new context((void*)zport, mrole);
	if(pshctx->rank == 0){ // the first worker 
	  if(dstnode == firstcoord){
	    assert(pshctx->star_sendportmap.size() < 2);
	    pshctx->star_sendportmap.insert(pair<int, _ringport*>(rackport , recvport));	  
	  }else{
	    assert(pshctx->star_sendportmap.size() < 2);
	    assert(dstnode == (pshctx->rank +1));
	    pshctx->star_sendportmap.insert(pair<int, _ringport*>(rdataport , recvport));	  
	  }
	}else if(pshctx->rank == (workermach-1)){ // the last worker 
	  if(dstnode == firstcoord){
	    assert(pshctx->star_sendportmap.size() < 2);
	    pshctx->star_sendportmap.insert(pair<int, _ringport*>(rdataport , recvport));	  
	  }else{
	    assert(pshctx->star_recvportmap.size() < 2);
	    assert(dstnode == (pshctx->rank - 1));
	    pshctx->star_sendportmap.insert(pair<int, _ringport*>(rackport , recvport));	  
	  }
	}else{ // workers between the first and last workers. 
	  int rank = pshctx->rank;
	  if(dstnode == (rank + 1) ){
	    assert(pshctx->star_recvportmap.size() < 2);
	    pshctx->star_sendportmap.insert(pair<int, _ringport*>(rdataport , recvport));	  
	  }else{
	    assert(pshctx->star_recvportmap.size() < 2);
	    assert(dstnode == (rank-1));
	    pshctx->star_sendportmap.insert(pair<int, _ringport*>(rackport , recvport));	  
	  }
	}
      }
    }

    worker_ringwakeup(pshctx, rdataport, rank);
    worker_ringwakeup(pshctx, rackport, rank);

  }else if(mrole == mrole_scheduler ){
    // do noting: schedulr
  }else{
    strads_msg(ERR, "[Fatal: rank %d] MACHINE TYPE IS NOT ASSIGNED YET \n", rank);
    assert(0);
  }

  MPI_Barrier(MPI_COMM_WORLD); 
}

void worker_ringwakeup(sharedctx *pshctx, int dackport, int rank){
    char *buffer = (char *)calloc(sizeof(char), MAX_BUFFER);
    sprintf(buffer, "Heart Beat from rank %d", rank);
    string msg(buffer);
    zmq::socket_t *sendport = pshctx->star_sendportmap[dackport]->ctx->m_zmqsocket;
    cpps_send(*sendport, (void *)msg.c_str(), strlen(msg.c_str()));    
    printf("COORDINATOR Rank(%d) Send HB to SEND PORT ptr -- socket(%p) -- Sending is DONE  \n", rank, sendport);
    int id = 1;
    zmq::socket_t *port_r = pshctx->star_recvportmap[dackport]->ctx->m_zmqsocket;       
    strads_msg(ERR, "[ Rank : %d ]  GOT MESSAGE recvport (%p) \n", rank, port_r);
    get_id_msg(*port_r, rank, &id, buffer, MAX_BUFFER);      
    strads_msg(ERR, "[ Worker Rank : %d ]  GOT MESSAGE recvport (%p) DONE DONE \n", rank, port_r);
    cpps_sendmore(*port_r, (void *)&id, 4);
    string msg1("Go");
    cpps_send(*port_r, (void *)msg1.c_str(), MAX_BUFFER-1);   
    get_single_msg(*sendport, rank, buffer, MAX_BUFFER);
    strads_msg(OUT, "[Worker Rank %d] got confirm for SEND PORT \n", rank);
}

void coordinator_ringwakeup(sharedctx *pshctx, int dackport, int rank){

    char *buffer = (char *)calloc(sizeof(char), MAX_BUFFER);
    sprintf(buffer, "Heart Beat from rank %d", rank);
    string msg(buffer);
    zmq::socket_t *sendport = pshctx->worker_sendportmap[dackport]->ctx->m_zmqsocket;
    cpps_send(*sendport, (void *)msg.c_str(), strlen(msg.c_str()));    
    printf("COORDINATOR Rank(%d) Send HB to SEND PORT ptr -- socket(%p) -- Sending is DONE  \n", rank, sendport);
    int id = 1;
    zmq::socket_t *port_r = pshctx->worker_recvportmap[dackport]->ctx->m_zmqsocket;
    strads_msg(ERR, "[ Coordinator Rank : %d ]  GOT MESSAGE recvport (%p) \n", rank, port_r);
    get_id_msg(*port_r, rank, &id, buffer, MAX_BUFFER);      
    strads_msg(ERR, "[ Coordinator Rank : %d ]  GOT MESSAGE recvport (%p) DONE DONE \n", rank, port_r);
    cpps_sendmore(*port_r, (void *)&id, 4);
    string msg1("Go");
    cpps_send(*port_r, (void *)msg1.c_str(), MAX_BUFFER-1);   
    get_single_msg(*sendport, rank, buffer, MAX_BUFFER);
    strads_msg(OUT, "[Coordinator Rank %d] got confirm for SEND PORT \n", rank);
}
























/////////////////////////////////////////////////////////////////////////////////////////////////////////////
// create_ringworker with coordinator.
// create a ring on workers and a coordinator nodes 
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void create_ringworker_ethernet_aux(sharedctx *pshctx, zmq::context_t &contextzmq, int mpi_size, string &cip)
{

  strads_msg(ERR, "\n\n\n[ Ring worker ring ] start\n");

  int hwm;
  size_t hwmsize = sizeof(hwm);
  int rank = pshctx->rank;
  int *idcnt_s = (int *)calloc(sizeof(int), MAX_MACH);
  memset((void *)idcnt_s, 0x0, sizeof(int)*MAX_MACH);
  int *idcnt_w = (int *)calloc(sizeof(int), MAX_MACH);
  memset((void *)idcnt_w, 0x0, sizeof(int)*MAX_MACH);
  mach_role mrole = pshctx->find_role(mpi_size);
  char *tmpcstring = (char *)calloc(sizeof(char), 128);
  int firstsched = pshctx->m_first_schedmach;
  //int firstworker = pshctx->m_first_workermach;
  int schedmach = pshctx->m_sched_machines;
  int workermach = pshctx->m_worker_machines;;
  int firstcoord = pshctx->m_first_coordinatormach; 

  assert(firstcoord == (mpi_size -1));

  if(mrole == mrole_coordinator){

    for(auto const &p : pshctx->links){
      mlink *tmp = p.second;

      if( (p.second->srcnode >= firstsched and p.second->srcnode <firstsched+schedmach) or // if workers related link, skip 
	  (p.second->dstnode >= firstsched and p.second->dstnode <firstsched+schedmach)){
	continue;
      }

      // for receiving port
      if(tmp->dstnode == pshctx->rank){
	int dstport = tmp->dstport; 
	int srcnode = tmp->srcnode;
	zmq::socket_t *pport_s = new zmq::socket_t(contextzmq, ZMQ_ROUTER);
	int sethwm = MAX_ZMQ_HWM;
	pport_s->setsockopt(ZMQ_RCVHWM, &sethwm, sizeof(int)); 
	sprintf(tmpcstring, "tcp://*:%d", dstport); 
	pport_s->bind (tmpcstring); // open 5555 for all incomping connection      
	pport_s->getsockopt(ZMQ_RCVHWM, (void *)&hwm, &hwmsize);
	strads_msg(ERR, "@@@@@@@ Coordinator rank %d open a port%s FOR RECEIVE PORT -- ptr to zmqsocket(%p) HWM(%d)\n", 
		   pshctx->rank, tmpcstring, pport_s, hwm);
	mach_role dstmrole = pshctx->find_role(mpi_size, srcnode);
	_ringport *recvport = new class _ringport;
	recvport->ctx = new context((void*)pport_s, mrole_coordinator);
	if(dstmrole == mrole_scheduler){
	  assert(0);
	}else if(dstmrole == mrole_worker){
	  if(srcnode == (workermach-1)){
	    assert(pshctx->ring_recvportmap.size() < 2);
	    pshctx->ring_recvportmap.insert(pair<int, _ringport*>(rdataport, recvport));	  	    
	  }else if(srcnode == 0){
	    assert(pshctx->ring_recvportmap.size() < 2);
	    pshctx->ring_recvportmap.insert(pair<int, _ringport*>(rackport, recvport));	  	    
	  }else{
	    assert(0);
	  }
	}else{
	  assert(0);
	}
      }

      // for sending port
      if(tmp->srcnode == pshctx->rank){
	int dstnode = tmp->dstnode;
	int dstport = tmp->dstport; 
	zmq::socket_t *zport = new zmq::socket_t(contextzmq, ZMQ_DEALER);
	int *identity = (int *)calloc(1, sizeof(int));
	*identity = 1;
	zport->setsockopt(ZMQ_IDENTITY, (void *)identity, sizeof(int));
	int sethwm = MAX_ZMQ_HWM;
	zport->setsockopt(ZMQ_SNDHWM, &sethwm, sizeof(int));
	sprintf(tmpcstring, "tcp://%s:%d", pshctx->nodes[dstnode]->ip.c_str(), dstport); 
	zport->connect (tmpcstring); // open 5555 for all incomping connection      
	strads_msg(ERR, "Rank %d CONNECT a port%s FOR SEND PORT -- ptr to socket(%p) \n", 
		   pshctx->rank, tmpcstring, zport);
	mach_role srcmrole = pshctx->find_role(mpi_size, dstnode);
	_ringport *sendport = new class _ringport;
	sendport->ctx = new context((void*)zport, mrole_coordinator);
	if(srcmrole == mrole_scheduler){
	  assert(0);
	}else if(srcmrole == mrole_worker){
	  if(dstnode == (workermach-1)){
	    assert(pshctx->ring_sendportmap.size() < 2);
	    pshctx->ring_sendportmap.insert(pair<int, _ringport*>(rackport, sendport));	  
	  }else if(dstnode == 0){
	    assert(pshctx->ring_sendportmap.size() < 2);
	    strads_msg(ERR, "coordinator put rdataport into sendport for dstnode %d (sendport[%p]\n", dstnode, sendport->ctx);
	    pshctx->ring_sendportmap.insert(pair<int, _ringport*>(rdataport, sendport));	  
	  }else{
	    assert(0);
	  }
	}else{
	  assert(0);
	}
      }
    }


    if(pshctx->ring_sendportmap.size() != 2){
      strads_msg(ERR, "ctx->rank %d breaks rules :  pshctx->ring_sendportmap.size() : %ld \n", 
		 pshctx->rank, pshctx->ring_sendportmap.size());
    }

    assert(pshctx->ring_sendportmap.size() == 2);
    assert(pshctx->ring_recvportmap.size() == 2);
    coordinator_ringwakeup_aux(pshctx, rdataport, rank);
    coordinator_ringwakeup_aux(pshctx, rackport, rank);
  }else if(mrole == mrole_worker ){

    //    int rcnt = 0;
    //    int scnt = 0;
    sleep(2);

    for(auto const &p : pshctx->links){

      mlink *tmp = p.second;
      if( (p.second->srcnode >= firstsched and p.second->srcnode <firstsched+schedmach) or // if scheduler related link, skip 
	  (p.second->dstnode >= firstsched and p.second->dstnode <firstsched+schedmach)){
	continue;
      }

      // for receiving port
      if(tmp->dstnode == pshctx->rank){
	int dstport = tmp->dstport; 
	int srcnode = tmp->srcnode;
	zmq::socket_t *pport_s = new zmq::socket_t(contextzmq, ZMQ_ROUTER);
	int sethwm = MAX_ZMQ_HWM;
	pport_s->setsockopt(ZMQ_RCVHWM, &sethwm, sizeof(int)); 
	sprintf(tmpcstring, "tcp://*:%d", dstport); 
	pport_s->bind (tmpcstring); // open 5555 for all incomping connection      
	pport_s->getsockopt(ZMQ_RCVHWM, (void *)&hwm, &hwmsize);
	strads_msg(ERR, "@@@@@@@ WORKER rank %d open a port%s FOR RECEIVE PORT -- ptr to zmqsocket(%p) HWM(%d)\n", 
		   pshctx->rank, tmpcstring, pport_s, hwm);
	//mach_role dstmrole = pshctx->find_role(mpi_size, srcnode);
	_ringport *recvport = new class _ringport;
	recvport->ctx = new context((void*)pport_s, mrole_coordinator);

	if(pshctx->rank == 0){ // the first worker 
	  if(srcnode == firstcoord){
	    assert(pshctx->ring_recvportmap.size() < 2);
	    pshctx->ring_recvportmap.insert(pair<int, _ringport*>(rdataport, recvport)); // for data port 	  
	  }else{
	    assert(pshctx->ring_recvportmap.size() < 2);
	    pshctx->ring_recvportmap.insert(pair<int, _ringport*>(rackport, recvport)); // for data port 	  
	  }
	}else if(pshctx->rank == (workermach-1)){ // the last worker 
	  if(srcnode == firstcoord){
	    assert(pshctx->ring_recvportmap.size() < 2);
	    pshctx->ring_recvportmap.insert(pair<int, _ringport*>(rackport, recvport)); // for data port 	  
	  }else{
	    assert(pshctx->ring_recvportmap.size() < 2);
	    assert(srcnode == (pshctx->rank - 1));
	    pshctx->ring_recvportmap.insert(pair<int, _ringport*>(rdataport, recvport)); // for data port 	  
	  }
	}else{ // workers between the first and last workers. 
	  int rank = pshctx->rank;
	  if(srcnode == (rank -1) ){
	    assert(pshctx->ring_recvportmap.size() < 2);
	    pshctx->ring_recvportmap.insert(pair<int, _ringport*>(rdataport, recvport)); // for data port 	  
	  }else{
	    assert(pshctx->ring_recvportmap.size() < 2);
	    pshctx->ring_recvportmap.insert(pair<int, _ringport*>(rackport, recvport)); // for data port 	  
	  }
	}
      }

      // for sending port
      if(tmp->srcnode == pshctx->rank){
	int dstnode = tmp->dstnode;
	int dstport = tmp->dstport; 
	zmq::socket_t *zport = new zmq::socket_t(contextzmq, ZMQ_DEALER);
	int *identity = (int *)calloc(1, sizeof(int));
	*identity = 1;
	zport->setsockopt(ZMQ_IDENTITY, (void *)identity, sizeof(int));
	int sethwm = MAX_ZMQ_HWM;
	zport->setsockopt(ZMQ_SNDHWM, &sethwm, sizeof(int));

	//sprintf(tmpcstring, "tcp://%s:%d", cip.c_str(), dstport); 
	sprintf(tmpcstring, "tcp://%s:%d", pshctx->nodes[dstnode]->ip.c_str(), dstport); 
	zport->connect (tmpcstring); // open 5555 for all incomping connection      
	strads_msg(ERR, "Rank %d CONNECT a port%s FOR SEND PORT -- ptr to socket(%p) \n", 
		   pshctx->rank, tmpcstring, zport);
	_ringport *recvport = new class _ringport;
	recvport->ctx = new context((void*)zport, mrole);
	if(pshctx->rank == 0){ // the first worker 
	  if(dstnode == firstcoord){
	    assert(pshctx->ring_sendportmap.size() < 2);
	    pshctx->ring_sendportmap.insert(pair<int, _ringport*>(rackport , recvport));	  
	  }else{
	    assert(pshctx->ring_sendportmap.size() < 2);
	    assert(dstnode == (pshctx->rank +1));
	    pshctx->ring_sendportmap.insert(pair<int, _ringport*>(rdataport , recvport));	  
	  }
	}else if(pshctx->rank == (workermach-1)){ // the last worker 
	  if(dstnode == firstcoord){
	    assert(pshctx->ring_sendportmap.size() < 2);
	    pshctx->ring_sendportmap.insert(pair<int, _ringport*>(rdataport , recvport));	  
	  }else{
	    assert(pshctx->ring_sendportmap.size() < 2);
	    assert(dstnode == (pshctx->rank - 1));
	    pshctx->ring_sendportmap.insert(pair<int, _ringport*>(rackport , recvport));	  
	  }
	}else{ // workers between the first and last workers. 
	  int rank = pshctx->rank;
	  if(dstnode == (rank + 1) ){
	    assert(pshctx->ring_sendportmap.size() < 2);
	    pshctx->ring_sendportmap.insert(pair<int, _ringport*>(rdataport , recvport));	  
	  }else{

	    //	    if(pshctx->ring_recvportmap.size() >= 2){
	    strads_msg(ERR, "FATAL : Rank(%d)  (%ld)   src(%d)  dst(%d)\n", 
		       pshctx->rank,
		       pshctx->ring_sendportmap.size(), 
		       p.second->srcnode, 
		       p.second->dstnode);   
	    //	    }	    
	    assert(pshctx->ring_sendportmap.size() < 2);
	    assert(dstnode == (rank-1));
	    pshctx->ring_sendportmap.insert(pair<int, _ringport*>(rackport , recvport));	  
	  }
	}
      }





    }

    worker_ringwakeup_aux(pshctx, rdataport, rank);
    worker_ringwakeup_aux(pshctx, rackport, rank);

  }else if(mrole == mrole_scheduler ){
    // do noting: schedulr
  }else{
    strads_msg(ERR, "[Fatal: rank %d] MACHINE TYPE IS NOT ASSIGNED YET \n", rank);
    assert(0);
  }

  MPI_Barrier(MPI_COMM_WORLD); 
}







void worker_ringwakeup_aux(sharedctx *pshctx, int dackport, int rank){
    char *buffer = (char *)calloc(sizeof(char), MAX_BUFFER);
    sprintf(buffer, "Heart Beat from rank %d", rank);
    string msg(buffer);
    zmq::socket_t *sendport = pshctx->ring_sendportmap[dackport]->ctx->m_zmqsocket;
    cpps_send(*sendport, (void *)msg.c_str(), strlen(msg.c_str()));    
    printf("COORDINATOR Rank(%d) Send HB to SEND PORT ptr -- socket(%p) -- Sending is DONE  \n", rank, sendport);
    int id = 1;
    zmq::socket_t *port_r = pshctx->ring_recvportmap[dackport]->ctx->m_zmqsocket;       
    strads_msg(ERR, "[ Rank : %d ]  GOT MESSAGE recvport (%p) \n", rank, port_r);
    get_id_msg(*port_r, rank, &id, buffer, MAX_BUFFER);      
    strads_msg(ERR, "[ Worker Rank : %d ]  GOT MESSAGE recvport (%p) DONE DONE \n", rank, port_r);
    cpps_sendmore(*port_r, (void *)&id, 4);
    string msg1("Go");
    cpps_send(*port_r, (void *)msg1.c_str(), MAX_BUFFER-1);   
    get_single_msg(*sendport, rank, buffer, MAX_BUFFER);
    strads_msg(OUT, "[Worker Rank %d] got confirm for SEND PORT \n", rank);
}

void coordinator_ringwakeup_aux(sharedctx *pshctx, int dackport, int rank){

    char *buffer = (char *)calloc(sizeof(char), MAX_BUFFER);
    sprintf(buffer, "Heart Beat from rank %d", rank);
    string msg(buffer);
    zmq::socket_t *sendport = pshctx->ring_sendportmap[dackport]->ctx->m_zmqsocket;
    cpps_send(*sendport, (void *)msg.c_str(), strlen(msg.c_str()));    
    printf("COORDINATOR Rank(%d) Send HB to SEND PORT ptr -- socket(%p) -- Sending is DONE  \n", rank, sendport);
    int id = 1;
    zmq::socket_t *port_r = pshctx->ring_recvportmap[dackport]->ctx->m_zmqsocket;
    strads_msg(ERR, "[ Coordinator Rank : %d ]  GOT MESSAGE recvport (%p) \n", rank, port_r);
    get_id_msg(*port_r, rank, &id, buffer, MAX_BUFFER);      
    strads_msg(ERR, "[ Coordinator Rank : %d ]  GOT MESSAGE recvport (%p) DONE DONE \n", rank, port_r);
    cpps_sendmore(*port_r, (void *)&id, 4);
    string msg1("Go");
    cpps_send(*port_r, (void *)msg1.c_str(), MAX_BUFFER-1);   
    get_single_msg(*sendport, rank, buffer, MAX_BUFFER);
    strads_msg(OUT, "[Coordinator Rank %d] got confirm for SEND PORT \n", rank);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void create_ps_star_ethernet(sharedctx *pshctx, zmq::context_t &contextzmq, int mpi_size, string &cip, int server_rank)
{
  int hwm, hwm_send;
  size_t hwmsize = sizeof(hwm);
  size_t hwm_send_size = sizeof(hwm_send);
  int rank = pshctx->rank;

  char *buffer = (char *)calloc(sizeof(char), MAX_BUFFER); // max message size is limited to 2048 bytes now                       
  int *idcnt_s = (int *)calloc(sizeof(int), MAX_MACH);
  memset((void *)idcnt_s, 0x0, sizeof(int)*MAX_MACH);

  int *idcnt_w = (int *)calloc(sizeof(int), MAX_MACH);
  memset((void *)idcnt_w, 0x0, sizeof(int)*MAX_MACH);

  mach_role mrole = pshctx->find_role(mpi_size);
  char *tmpcstring = (char *)calloc(sizeof(char), 128);

  if(mrole == mrole_scheduler){ // ps server 

    int schedcnt=0;
    int workercnt=0;
    int schedcnt_r=0;
    int workercnt_r=0;   

    for(auto const &p : pshctx->ps_links){
      mlink *tmp = p.second;

      // for receiving port
      if(tmp->dstnode == pshctx->rank){
	int dstport = tmp->dstport; 
	int srcnode = tmp->srcnode;
	zmq::socket_t *pport_s = new zmq::socket_t(contextzmq, ZMQ_ROUTER);
	int sethwm = MAX_ZMQ_HWM;
	pport_s->setsockopt(ZMQ_RCVHWM, &sethwm, sizeof(int)); 
	sprintf(tmpcstring, "tcp://*:%d", dstport); 
	pport_s->bind (tmpcstring); // open 5555 for all incomping connection      
	pport_s->getsockopt(ZMQ_RCVHWM, (void *)&hwm, &hwmsize);
	strads_msg(ERR, "@@@@@@@ PS rank %d open a port%s FOR RECEIVE PORT -- ptr to zmqsocket(%p) HWM(%d)\n", 
		   pshctx->rank, tmpcstring, pport_s, hwm);
	mach_role dstmrole = pshctx->find_role(mpi_size, srcnode);
	_ringport *recvport = new class _ringport;
	recvport->ctx = new context((void*)pport_s, mrole_scheduler);
	if(dstmrole == mrole_scheduler){
	  assert(0);
	  //	  pshctx->ps_recvportmap.insert(pair<int, _ringport*>(schedcnt, recvport));	  
	  //	  schedcnt++;
	}else if(dstmrole == mrole_worker){
	  pshctx->ps_recvportmap.insert(pair<int, _ringport*>(workercnt, recvport));	  
	  workercnt++;
	}else{

	  strads_msg(ERR, "PS [Fatal] SRC NODE (%d)[dstmrole: %d] rank %d \n", srcnode, dstmrole, pshctx->rank);
	  assert(0);
	}
      }

      // for sending port
      if(tmp->srcnode == pshctx->rank){
	int srcport = tmp->srcport; 
	int dstnode = tmp->dstnode;
	zmq::socket_t *pport_s = new zmq::socket_t(contextzmq, ZMQ_ROUTER);
	int sethwm = MAX_ZMQ_HWM;
	pport_s->setsockopt(ZMQ_SNDHWM, &sethwm, sizeof(int)); 
	pport_s->setsockopt(ZMQ_RCVHWM, &sethwm, sizeof(int)); 

	sprintf(tmpcstring, "tcp://*:%d", srcport); 
	pport_s->bind (tmpcstring);      
	pport_s->getsockopt(ZMQ_RCVHWM, (void *)&hwm, &hwmsize);
	pport_s->getsockopt(ZMQ_SNDHWM, (void *)&hwm_send, &hwm_send_size);
	strads_msg(ERR, "PS rank %d open a port%s FOR SEND PORT -- ptr to zmqsocket(%p) RCVHWM(%d) SNDHWM(%d)\n", 
		   pshctx->rank, tmpcstring, pport_s, hwm, hwm_send);
	mach_role srcmrole = pshctx->find_role(mpi_size, dstnode);
	_ringport *sendport = new class _ringport;
	sendport->ctx = new context((void*)pport_s, mrole_scheduler);
	if(srcmrole == mrole_scheduler){
	  assert(0);
	  //	  pshctx->ps_sendportmap.insert(pair<int, _ringport*>(schedcnt_r, sendport));	  
	  //	  schedcnt_r++;
	}else if(srcmrole == mrole_worker){
	  pshctx->ps_sendportmap.insert(pair<int, _ringport*>(workercnt_r, sendport));	  
	  workercnt_r++;
	}else{
	  assert(0);
	}
      }
    }

    strads_msg(ERR, "[PS] Open ports and start hands shaking worker_recvportsize(%ld) worker_sendportsize(%ld)\n", 
	       pshctx->worker_recvportmap.size(),
	       pshctx->worker_sendportmap.size());

    for(unsigned int i=0; i<pshctx->ps_recvportmap.size(); i++){
      int id = 1;
      zmq::socket_t *port_r = pshctx->ps_recvportmap[i]->ctx->m_zmqsocket;
      strads_msg(ERR, "PS -- wait for %dth worker RECV out of  %ld  pport(%p)\n", 
		 i, pshctx->ps_sendportmap.size(), port_r);

      get_id_msg(*port_r, rank, &id, buffer, MAX_BUFFER);      
      assert(id == 1);
      cpps_sendmore(*port_r, (void *)&id, 4);
      string msg1("Go");
      cpps_send(*port_r, (void *)msg1.c_str(), MAX_BUFFER-1);   
    }

    strads_msg(ERR, "[PS @@@@@] finish worker recv port confirm \n");

    for(unsigned int i=0; i<pshctx->ps_sendportmap.size(); i++){
      int id = 0;
      zmq::socket_t *port_r = pshctx->ps_sendportmap[i]->ctx->m_zmqsocket;
      strads_msg(ERR, "Coordinator -- wait for %dth worker SENDPORT out of  %ld  pport(%p)\n", 
		 i, pshctx->ps_sendportmap.size(), port_r);
      get_id_msg(*port_r, rank, &id, buffer, MAX_BUFFER);      
      cpps_sendmore(*port_r, (void *)&id, 4);
      string msg1("Go");
      cpps_send(*port_r, (void *)msg1.c_str(), MAX_BUFFER-1);   
    }
    strads_msg(ERR, "[PS @@@@@] finish worker send port confirm \n");
      

  }else if(mrole == mrole_worker){

    sleep(2);

    int slotid_r = pshctx->ps_recvportmap.size();
    int slotid_s = pshctx->ps_sendportmap.size();    

    for(auto const &p : pshctx->ps_links){
      mlink *tmp = p.second;

      // for receiving port
      if(tmp->dstnode == pshctx->rank and tmp->srcnode == server_rank ){
	int srcport = tmp->srcport; 
	zmq::socket_t *zport = new zmq::socket_t(contextzmq, ZMQ_DEALER);
	int *identity = (int *)calloc(1, sizeof(int));
	*identity = 1;
	zport->setsockopt(ZMQ_IDENTITY, (void *)identity, sizeof(int));

	int *sethwm = (int *)calloc(1, sizeof(int)); 
	*sethwm = MAX_ZMQ_HWM;
	zport->setsockopt(ZMQ_RCVHWM, sethwm, sizeof(int)); 

	//	sprintf(tmpcstring, "tcp://10.54.1.30:%d", srcport); 
	sprintf(tmpcstring, "tcp://%s:%d", cip.c_str(), srcport); 
	zport->connect (tmpcstring); // open 5555 for all incomping connection      
	zport->getsockopt(ZMQ_RCVHWM, (void *)&hwm, &hwmsize);
	strads_msg(ERR, "RANK rank %d CONNECT a port  %s FOR RECEIVE PORT -- ptr to socket(%p) HWM(%d) \n", 
		   pshctx->rank, tmpcstring, zport, hwm);
	_ringport *recvport = new class _ringport;
	recvport->ctx = new context((void*)zport, mrole);

	int slotid = pshctx->ps_recvportmap.size();
	assert(slotid == slotid_r);
	pshctx->ps_recvportmap.insert(pair<int, _ringport*>(slotid_r, recvport));	  
      }

      // for sending port
      if(tmp->srcnode == pshctx->rank and tmp->dstnode == server_rank){
	int dstport = tmp->dstport; 
	zmq::socket_t *zport = new zmq::socket_t(contextzmq, ZMQ_DEALER);
	int *identity = (int *)calloc(1, sizeof(int));
	*identity = 1;
	zport->setsockopt(ZMQ_IDENTITY, (void *)identity, sizeof(int));
	int sethwm = MAX_ZMQ_HWM;
	zport->setsockopt(ZMQ_SNDHWM, &sethwm, sizeof(int)); 
	//	sprintf(tmpcstring, "tcp://10.54.1.30:%d", dstport); 
	sprintf(tmpcstring, "tcp://%s:%d", cip.c_str(), dstport); 
	zport->connect (tmpcstring); // open 5555 for all incomping connection      
	strads_msg(ERR, "Rank %d CONNECT a port%s FOR SEND PORT -- ptr to socket(%p) \n", 
		   pshctx->rank, tmpcstring, zport);
	_ringport *recvport = new class _ringport;
	recvport->ctx = new context((void*)zport, mrole);

	int slotid = pshctx->ps_sendportmap.size();
	assert(slotid == slotid_s);
	pshctx->ps_sendportmap.insert(pair<int, _ringport*>(slotid, recvport));	  
      }
    }

    assert((slotid_r+1) == pshctx->ps_recvportmap.size());
    assert((slotid_s+1) == pshctx->ps_sendportmap.size());    

    char *buffer = (char *)calloc(sizeof(char), MAX_BUFFER);
    sprintf(buffer, "PS Heart Beat from rank %d", rank);
    string msg(buffer);
    zmq::socket_t *sendport = pshctx->ps_sendportmap[slotid_s]->ctx->m_zmqsocket;
    printf("PS Rank(%d) Send HB to SEND PORT ptr -- socket(%p) \n", rank, sendport);
    cpps_send(*sendport, (void *)msg.c_str(), strlen(msg.c_str()));    
    get_single_msg(*sendport, rank, buffer, MAX_BUFFER);
    strads_msg(OUT, "PS [Rank %d] got confirm for RECV PORT \n", rank);
    zmq::socket_t *recvport = pshctx->ps_recvportmap[slotid_r]->ctx->m_zmqsocket;
    printf("PS Rank(%d) Send HB to RECV PORT ptr -- socket(%p) \n", rank, recvport);
    cpps_send(*recvport, (void *)msg.c_str(), strlen(msg.c_str()));    
    get_single_msg(*recvport, rank, buffer, MAX_BUFFER);
    strads_msg(OUT, "PS [Rank %d] got confirm for SEND PORT \n", rank);

  }else{
    strads_msg(ERR, "PS [Fatal: rank %d] MACHINE TYPE IS NOT ASSIGNED YET \n", rank);
    assert(0);
  }

  strads_msg(ERR, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ PSPS finish create star  ethernet by rank (%d)  with server rank %d\n", 
	     rank, server_rank);

  //  MPI_Barrier(MPI_COMM_WORLD); 
}
