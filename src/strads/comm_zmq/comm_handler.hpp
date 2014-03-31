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
#ifndef __COMM_HANDLER_HPP__
#define __COMM_HANDLER_HPP__
#include <stdio.h>
#include <boost/scoped_ptr.hpp>
#include <zmq.hpp>
#include <pthread.h>
#include <string>
#include <stdint.h>
#include <queue>
#include <semaphore.h>
#include <boost/unordered_map.hpp>
#include <boost/shared_array.hpp>
#include <boost/thread/tss.hpp>
#include <boost/scoped_array.hpp>
/*
#define DBG 0  // print when debugging
#define ERR 1  // print when error should be exposed
#define NOR 2  // always print
*/

/*
 * TODO:
 * This code has become too complicated with the functionalities added after initial design. A code refactor is needed.
 *
 * 1. Add message format -- use 0mq's multi-part message.
 *   1) Except for outgoing messages, all other messages (internal messages) do not need to send a prefix (destination id
 *      or publish group id). So other messages should use send_msg/recv_msg without cliid_t but with flag set to MORE.
 *      They should follow the following format:
 *      message_type - an enum
 *      other parts...
 *
 *      Thus inter-thread sockets should be reduced to only one.
 *   2) A better logging system.
 *
 * */

#define MSGQ_ENDP "inproc://msgq"
#define TASKQ_ENDP "inproc://taskq"
#define SHUTDOWN_ENDP "inproc://shutdown"
#define CONN_ENDP "inproc://conn"
#define PUB_SEND_ENDP "inproc://pub_send"
#define SUB_PULL_ENDP "inproc://sub_pull"
#define PUB_START_ENDP "inproc://pub_start"
#define SUBER_Q_ENDP "inproc://suber_q"


#ifndef COMM_HANDLER_LOGLEVEL
//#define COMM_HANDLER_LOGLEVEL 0
#define COMM_HANDLER_LOGLEVEL 1
#endif

#define IDE2I(x) (x << 4 | 0x1)
#define IDI2E(x) (x >> 4)


#define LOG(LEVEL, OUT, FMT, ARGS...) if(LEVEL >= COMM_HANDLER_LOGLEVEL) {}
//#define LOG(LEVEL, OUT, FMT, ARGS...) fprintf(OUT, " "); 
//if(LEVEL >= COMM_HANDLER_LOGLEVEL) fprintf(OUT, "[%s][%s][ln:%d]  "##ARGS)


namespace commtest{
  
  typedef int32_t cliid_t;

  const int DBG = 0;  // print when debugging
  const int ERR = 1;  // print when error should be exposed
  const int NOR = 2;  // always print
  
  template<class T>
  class pcqueue{ //producer-consumer queue
  private:
    std::queue<T> q;
    pthread_mutex_t mutex;
    sem_t sem;
  public:
    pcqueue(){
      pthread_mutex_init(&mutex, NULL);
      sem_init(&sem, 0, 0);
    }

    ~pcqueue(){
      pthread_mutex_destroy(&mutex);
      sem_destroy(&sem);
    }

    void push(T t){
      pthread_mutex_lock(&mutex);
      q.push(t);
      pthread_mutex_unlock(&mutex);
      sem_post(&sem);
 
    }

    T pop(){
      sem_wait(&sem); //semaphore lets no more threads in than queue size
      pthread_mutex_lock(&mutex);
      T t = q.front();
      q.pop();
      pthread_mutex_unlock(&mutex);
      return t;
    }
  };

  struct config_param_t{
    cliid_t id; //must be set; others are optional
    bool accept_conns; //default false
    std::string ip;
    std::string port;

    config_param_t():
      id(0),
      accept_conns(false),
      ip("localhost"),
      port("9000"){}

    config_param_t(cliid_t _id, bool _accept_conns, std::string _ip, std::string _port):
      id(_id),
      accept_conns(_accept_conns),
      ip(_ip),
      port(_port){}
  };
  
  //TODO: add two callbacks 1) connection loss 2) error state
  //TODO: add funciton to clear error state
  

  /*
   * PGM (Pragmatic General Multicast) is not supported by PDL's cluster, so the PGM multicast code is commented out.
   * But they should work.
   * Currently multicast is implemented using 0mq's pub-sub communication pattern.
   * */

  class comm_handler {
  private:
    zmq::context_t zmq_ctx;

    // used for comm thread to communicate with worker threads, like make conn
    // a pull socket; application threads push to a corresponding socket
    zmq::socket_t msgq;
    // a push socket; application threads pull from a corresponding socket
    zmq::socket_t taskq;
    zmq::socket_t conn_sock;

    zmq::socket_t shutdown_sock; // a pull socket; if read anything, then shut down
    zmq::socket_t router_sock;
    zmq::socket_t monitor_sock;

    //shared among worker threads, should be protected by mutex
    boost::thread_specific_ptr<zmq::socket_t> msgpush;
    boost::thread_specific_ptr<zmq::socket_t> taskpull;
    boost::thread_specific_ptr<zmq::socket_t> connpush;

    zmq::socket_t pub_sendpull;
    boost::thread_specific_ptr<zmq::socket_t> pub_sendpush;

    //used to establish publish socket
    zmq::socket_t pubstart_pull;
    boost::thread_specific_ptr<zmq::socket_t> pubstart_push;

    //queue to wait for client
    zmq::socket_t suberq;
    boost::thread_specific_ptr<zmq::socket_t> suberq_pull;

    //used to establish subscription
    zmq::socket_t subpull;
    boost::thread_specific_ptr<zmq::socket_t> subpush;

    //used to synchronize

    zmq::socket_t pub_sock;
    zmq::socket_t sub_sock;

    pthread_mutex_t sync_mtx;
    pthread_t pthr;
    cliid_t id;
    std::string ip;
    std::string port;
    int errcode; // 0 is no error
    pcqueue<cliid_t> clientq;
    boost::unordered_map<cliid_t, bool> clientmap;
    boost::unordered_map<cliid_t, bool> publishermap;
    bool accept_conns;
    static void *start_handler(void *_comm);

    // TODO: we can allow clients to join multicast group after their communication threads start running, we do it this
    // way just for simpilicity
    //int init_multicast(config_param_t &cparam);

  public:
    
    comm_handler(config_param_t &cparam):
      zmq_ctx(1),
      msgq(zmq_ctx, ZMQ_PULL),
      taskq(zmq_ctx, ZMQ_PUSH),
      conn_sock(zmq_ctx, ZMQ_PULL),
      shutdown_sock(zmq_ctx, ZMQ_PULL),
      router_sock(zmq_ctx, ZMQ_ROUTER),
      monitor_sock(zmq_ctx, ZMQ_PAIR),
      pub_sendpull(zmq_ctx, ZMQ_PULL),
      pubstart_pull(zmq_ctx, ZMQ_PULL),
      suberq(zmq_ctx, ZMQ_PUSH),
      subpull(zmq_ctx, ZMQ_PULL),
      pub_sock(zmq_ctx, ZMQ_PUB),
      sub_sock(zmq_ctx, ZMQ_SUB),
      id(IDE2I(cparam.id)),
      ip(cparam.ip),
      port(cparam.port),
      accept_conns(cparam.accept_conns){
      pthread_mutex_init(&sync_mtx, NULL);
      try{	
        msgq.bind(MSGQ_ENDP);
        taskq.bind(TASKQ_ENDP);
        conn_sock.bind(CONN_ENDP);
        shutdown_sock.bind(SHUTDOWN_ENDP);
        pub_sendpull.bind(PUB_SEND_ENDP);
        pubstart_pull.bind(PUB_START_ENDP);
        suberq.bind(SUBER_Q_ENDP);
        subpull.bind(SUB_PULL_ENDP);
      }catch(zmq::error_t &e){
        LOG(DBG, stderr, "Failed to bind to inproc socket: %s\n", e.what());
        throw std::bad_alloc();
      }

      pthread_mutex_lock(&sync_mtx);
      errcode = 0;
      int ret = pthread_create(&pthr, NULL, start_handler, this);
      if(ret != 0) throw std::bad_alloc();
      pthread_mutex_lock(&sync_mtx); //cannot return until comm thread starts running
      pthread_mutex_unlock(&sync_mtx);
      if(errcode) throw std::bad_alloc();
    }
    
    ~comm_handler(){
      pthread_mutex_destroy(&sync_mtx);
    }

    /*
     * Usage:
     * If config_param_t::accept_conns is true, comm_handler may accept connections, otherwise, it may not.
     * Note that if a comm_handler may accept connections, it accepts connections all the time, not only
     * when get_one_connection() is called.
     *
     * For comm_handler to be a publisher, it must:
     * 1. have config_param_t::accept_conns set, so comm_handler may accept connections.
     * 2. call init_pub once with publication ip and port.
     * Note that init_pub may be called multiple times, but only the first may (and must) have ip and port set.
     *
     * For comm_handler to subscribe to a publication group, it must:
     * 1. first, call connect_to to connect to publisher's unicast socket;
     * 2. second, call subscribe_to, to subscribe to one or publication groups.
     * 3. subscribe_to may be called multipule times, but each ip/port pair may only be used once.
     *
     * */

    
    cliid_t get_one_connection();
    int connect_to(std::string destip, std::string destport, cliid_t destid);
    // pubids should be different from any node ids
    int subscribe_to(std::string pubip, std::string pubport, std::vector<cliid_t> pubids);

    // does not return until get enough subscribers
    // can only be called once with ip and port set
    int init_pub(std::string ip, std::string port, cliid_t gid, int num_subs);

    // used by a publisher to publish a message
    int publish(cliid_t group, uint8_t *data, size_t len);
    // thread-safe send, recv, recv_async
    int send(cliid_t cid, uint8_t *data, size_t len); //non-blocking send
    int recv(cliid_t &cid, boost::shared_array<uint8_t> &data); //blocking recv
    int recv_async(cliid_t &cid, boost::shared_array<uint8_t> &data); // nonblocking recv
    int shutdown();
  };
}
#endif
