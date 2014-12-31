#pragma once 

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <unordered_map>

#include <strads/util/utility.hpp>
#include <strads/include/strads-macro.hpp>
#include <strads/include/common.hpp>
#include <strads/include/indepds.hpp>
#include <zmq.hpp>
#include <strads/netdriver/comm.hpp>

//#define MAX_ZMQ_HWM (2000)
#define MAX_PULL_OUT (128)


#define rdataport  0
#define rackport   1


typedef struct{
  long seqno;
  uint64_t timestamp;
  long crc;
}testpkt;

struct qentry{
  unsigned long int timstamp;
  mbuffer *copied; // non-system registered one. 
};

class context{
public:
  context(void *zmqsocket, mach_role mrole):  m_mrole(mrole), m_ready(false) {
    m_fgsend = true;
    m_fgrecv = true;
    m_send_ctx = NULL;
    m_zmqsocket = (zmq::socket_t *)zmqsocket;
  }

  zmq::socket_t *m_zmqsocket;
  qentry *alloc_qep(void){   
    return NULL;
  }

  void free_qep(qentry *qep){   
    return;
  }
  int rank; // machine rank 
  //  roletype role;

  void set_fast_channel_endpoint_sender(void){   
    m_fgsend = true;
  }

  void set_fast_channel_intermediate_sender(void){   
    m_fgsend = false;
  }

  void set_fast_channel_endpoint_receiver(void){   
    m_fgrecv = true;
  }

  void set_fast_channel_intermediate_receiver(void){   
    m_fgrecv = false;
  }

  void set_direct_send_port(context *send_ctx){   
    m_send_ctx = send_ctx;
  }

  void get_inq_lock(void){
    int r = pthread_mutex_lock(&m_inq_lock);
    checkResults("[rdma buffer pool] pthread_mutex_lock failed", r);   
  }

  void release_inq_lock(void){
    int r = pthread_mutex_unlock(&m_inq_lock);
    checkResults("[rdma buffer pool] pthread_mutex_lock failed", r);   
  }

  void get_outq_lock(void){
    int r = pthread_mutex_lock(&m_outq_lock);
    checkResults("[rdma buffer pool] pthread_mutex_lock failed", r);   
  }

  void release_outq_lock(void){
    int r = pthread_mutex_unlock(&m_outq_lock);
    checkResults("[rdma buffer pool] pthread_mutex_lock failed", r);   
  }

  char *get_id_msg_async(zmq::socket_t &zport, int *len) {
    char *ret = NULL;
    for(int i=0; i< 2; i++){
      zmq::message_t request;
      if(i == 0){
	int bytes = zport.recv(&request, ZMQ_NOBLOCK);
	if(bytes == 0){
	  if(zmq_errno() == EAGAIN){
	    return NULL;
	  }else{
	    assert(0); // zmq error                                                               
	    return NULL;
	  }
	}
      }else{ // i == 1                                                                                             
	zport.recv(&request); // once id message is received successfully, call blocking receive....                    
      }
      char *data = (char *)request.data();
      int size = request.size();
      if(i == 0){
	assert(size != 0);
	assert(size == 4); // rank id has 4 bytes size                                                                      
      }
      if(i == 1){
	//	assert(len >= size);
	ret = (char *)calloc(sizeof(char), size);
	memcpy(ret, &data[0], size);
	*len = size;	
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
    return ret;
  }

  char *get_single_msg_async(zmq::socket_t &zport, int *len) {
    char *ret = NULL;
    zmq::message_t request;
    int bytes = zport.recv(&request, ZMQ_NOBLOCK);
    if(bytes == 0){
      if(zmq_errno() == EAGAIN){
	return NULL;
      }else{
	assert(0); // zmq error    
	return NULL;
      }
    }
    char *data = (char *)request.data();
    int size = request.size();
    ret = (char *)calloc(sizeof(char), size);
    *len = size;
    memcpy(ret, &data[0], size);
    //    msglen = size;
    int more;
    size_t more_size = sizeof (more);
    zport.getsockopt (ZMQ_RCVMORE, (void *)&more, &more_size);
    if (more){
      assert(0);
    }
    return ret;
  }

  void *pull_entry_inq(void){
    int len = -1;
    char *ret;
    if(m_mrole == mrole_coordinator){
      ret = get_id_msg_async(*m_zmqsocket, &len);
    }else{
      ret = get_single_msg_async(*m_zmqsocket, &len);
    }
    return (void *)ret;
  }

  void *pull_entry_inq(int *rlen){
    int len = -1;
    char *ret;
    if(m_mrole == mrole_coordinator){
      ret = get_id_msg_async(*m_zmqsocket, &len);
    }else{
      ret = get_single_msg_async(*m_zmqsocket, &len);
    }
    *rlen = len;
    return (void *)ret;
  }

  // caller : one associated with listener 
  int push_entry_outq(void *data, unsigned long int len){
    int id = 1;  
    if(m_mrole == mrole_coordinator){
      cpps_sendmore(*m_zmqsocket, &id, 4);
      cpps_send(*m_zmqsocket, data, len);
    }else{
      cpps_send(*m_zmqsocket, data, len);      
    }
    return 0;
  }


  // caller : one associated with listener 
  int push_ps_entry_outq(void *data, unsigned long int len){
    int id = 1;  
    if(m_mrole == mrole_scheduler){ // TODO : later change to mrole_ps_server
      cpps_sendmore(*m_zmqsocket, &id, 4);
      cpps_send(*m_zmqsocket, data, len);
    }else{
      cpps_send(*m_zmqsocket, data, len);      
    }
    return 0;
  }

  void *pull_ps_entry_inq(int *rlen){
    int len = -1;
    char *ret;
    if(m_mrole == mrole_scheduler){ // TODO : later change to mrole_ps_server
      ret = get_id_msg_async(*m_zmqsocket, &len);
    }else{
      ret = get_single_msg_async(*m_zmqsocket, &len);
    }
    *rlen = len;
    return (void *)ret;
  }



  void *ring_pull_entry_inq(void){
    int len = -1;
    char *ret;
    //    ret = get_single_msg_async(*m_zmqsocket, &len);
    ret = get_id_msg_async(*m_zmqsocket, &len);
    return (void *)ret;
  }

  void *ring_pull_entry_inq(int *rlen){
    int len = -1;
    char *ret;
    //    ret = get_single_msg_async(*m_zmqsocket, &len);
    ret = get_id_msg_async(*m_zmqsocket, &len);
    *rlen = len;
    return (void *)ret;
  }

  // caller : one associated with listener 
  int ring_push_entry_outq(void *data, unsigned long int len){
    //int id = 1;  
    cpps_send(*m_zmqsocket, data, len);      
    return 0;
    //    cpps_sendmore(*m_zmqsocket, &id, 4);
    //    cpps_send(*m_zmqsocket, data, len);
    return 0;
  }

  // caller : one associated with listener 
  int push_entry_inq(void *data, unsigned long int len){
    assert(0);
    return 0;
  }

  void *pull_entry_outq(void){
    void *rp = NULL;
    assert(0);
    return rp;
  }

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

  void release_buffer(void *data){
    free(data);
  }

  void set_connected_flag(bool flag){ 
  }

  bool get_connected_flag(void){
    return false;
  }

  // valid only for server port (receiving port)
  void set_server_ready_flag(bool flag){ 
  }

  // valid only for server port (receiving port)
  bool get_server_ready_flag(void){
    return false;
  }

  bool m_fgsend; // only for sender 
  bool m_fgrecv; // only for receiver 
  context *m_send_ctx;
  mach_role m_mrole;

private:

  // only poll_cq in listenser side cal this from time to time 
  void reclaim_buffer(void){
    int r = pthread_mutex_lock(&m_return_lock);
    checkResults("[rdma buffer pool] pthread_mutex_lock failed", r);         
    for(auto it = m_return_buffers.begin(); it != m_return_buffers.end(); it++){
      m_free_buffers.push_back(*it);            
      m_free_buffers_size++;
    }
    m_return_buffers.erase(m_return_buffers.begin(), m_return_buffers.end()); 
    r = pthread_mutex_unlock(&m_return_lock);
    checkResults("[rdma buffer pool] pthread_mutex_lock failed", r);         
  }

  pthread_mutex_t m_inq_lock;
  pthread_mutex_t m_outq_lock;
  pthread_mutex_t m_return_lock;
  unsigned long int m_free_buffers_size; 
  // do not use m_free_buffers.size() ... its complexity is logarithms -- suspicious.
  unsigned long int m_return_buffers_size;
  bool m_connected; // if connection is set up, connected flag is set to true 
  pthread_mutex_t m_connected_lock;
  std::list<struct qentry *>m_inq;
  std::list<struct qentry *>m_outq;
  std::list<mbuffer *>m_free_buffers;   // only for inq only
  std::list<mbuffer *>m_return_buffers; // only for inq only

  unsigned long int m_stime;
  unsigned long int m_etime;
  unsigned long int m_wtime;
  unsigned long int m_copycnt;
  unsigned long int m_lstime;
  unsigned long int m_letime;
  unsigned long int m_lwtime;
  unsigned long int m_rstime;
  unsigned long int m_retime;
  unsigned long int m_rwtime;
  pthread_mutex_t m_qep_lock;
  std::list<qentry *>m_qep_buffers;
  int m_outq_size;
  pthread_mutex_t m_ready_lock;
  bool m_ready;
};

void create_star_ethernet(sharedctx *pshctx, zmq::context_t &contextzmq, int mpi_size, std::string &cip);
void create_star_ethernet_singlemach(sharedctx *pshctx, zmq::context_t &contextzmq, int mpi_size);
void create_ring_ethernet(sharedctx *pshctx, zmq::context_t &contextzmq, int mpi_size, std::string &cip);
void create_ringworker_ethernet(sharedctx *pshctx, zmq::context_t &contextzmq, int mpi_size, std::string &cip);
void create_ringworker_ethernet_aux(sharedctx *pshctx, zmq::context_t &contextzmq, int mpi_size, std::string &cip);

void create_ps_star_ethernet(sharedctx *pshctx, zmq::context_t &contextzmq, int mpi_size, std::string &cip, int server_rank);
// create ring on worker and coordinator. 
