/********************************************************
  @author: Jin Kyu Kim (jinkyuk@cs.cmu.edu)
  @desc: Warpper functions for the communication primitives 
         It provides common abstractions on top of 
         various communication library - MPI, ZMQ, RDMA

********************************************************/

#if !defined(COMM_HPP_)
#define COMM_HPP_

#include <ifaddrs.h>
#include <netinet/in.h> 
#include <string.h> 
#include <arpa/inet.h>

//#include <boost/crc.hpp>
#include <assert.h>
#include <queue>
#include <iostream>     // std::cout
#include <algorithm>    // std::next_permutation, std::sort
#include <vector>       // std::vector
#include <ctime>        // std::time
#include <cstdlib>      // std::rand, std::srand
#include <list>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <string.h>

//#include <boost/multi_index_container.hpp>
//#include <boost/multi_index/member.hpp>
//#include <boost/multi_index/ordered_index.hpp>
//#include <boost/shared_array.hpp>

#define USER_MSG_SIZE (16*1024-72)
//#define USER_MSG_SIZE (16*1024-64)
// 56: size of type ... len except data in mbuffer struct
#define SYS_PACKET_LIMIT (USER_MSG_SIZE - 8)

enum mtype {
    MSG_MR,
    MSG_DONE,
    MSG_USER
};

enum message_type{
  SYSTEM_HB,
  USER_DATA,
  SYSTEM_DSHARD,
  SYSTEM_SCHEDULING,
  USER_WORK,
  USER_UPDATE, 
  USER_PROGRESS_CHECK
};

enum sys_ptype{
  SYS_DSHARD = 100
};

typedef struct{
  mtype type;
  unsigned int long seqno;
  unsigned int long timestamp;
  long cmdid;
  message_type msg_type;
  int src_rank;
  int dst_rank;
  unsigned int long len; // user can specify lentgh of valid bytes in data   
  int64_t remain;
  char data[USER_MSG_SIZE];
}mbuffer;


typedef struct{
  sys_ptype type;
  char syscmd[USER_MSG_SIZE -8];
}sys_packet;

#include <strads/include/common.hpp>

#include <strads/netdriver/zmq/zmq-common.hpp>

//void *com_put_syspacket(void *data, int size, sys_ptype);
// for ring topology
void com_start_server(sharedctx &shctx, uint16_t port, bool fastring);
void com_start_client(sharedctx &shctx, uint16_t port, std::string &s_ip, bool fastring, uint16_t cport);


// for star topology
void com_start_star_server(sharedctx &shctx, uint16_t port, bool fastring);
void com_start_star_client(sharedctx &shctx, uint16_t port, std::string &s_ip, bool fastring, uint16_t cport);

// common communication interface for all native comm handlers 
uint64_t setup_link(void *); // set up a link between two nodes 
uint64_t send_msg(void *);   // send message. put the msg on the buffer
uint64_t arecv_msg(void *);  // asynchronous receive 
uint64_t srecv_msg(void *);  // synchronous receive 

enum enum_role{ server, client, idle };

typedef struct{
  sharedctx *pshctx;
  uint16_t port; // peer's port 
  std::string *ps_ip; // peer's ip address
  enum_role role; 
  bool fastring;
  uint16_t cport; // local port 
}targ;

class _ringport{
public:  
  class context *ctx;
};


void *send_thread(void *parg);
void *recv_thread(void *parg);

void *send_star_thread(void *parg);
void *recv_star_thread(void *parg);

void parse_nodefile(std::string &fn, sharedctx &shctx);
void parse_starlinkfile(std::string &fn, sharedctx &shctx);
void parse_ps_nodefile(std::string &fn, sharedctx &shctx);
void parse_ps_linkfile(std::string &fn, sharedctx &shctx);
#endif 
