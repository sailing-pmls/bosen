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

//#include "com/comm.hpp"

#include "./include/common.hpp"
#include "com/zmq/zmq-common.hpp"
#include "com/comm.hpp"

#include "zmq.hpp"
#include <string>

using namespace std;

#define MAX_BUFFER (1024*16)
#define MAX_MACH (128)
#define ITER_TEST (0)
            
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

  MPI_Barrier(MPI_COMM_WORLD); 
}
