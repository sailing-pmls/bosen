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
#include "comm_handler.hpp"
#include <assert.h>
#include "zmq_util.hpp"
#include <unistd.h>
#include <sstream>

#define COMM_HANDLER_INIT "comm_handler_hello"
#define COMM_HANDLER_PUB_INIT "pub_hello"
#define COMM_HANDLER_SUB_INIT "sub_hello"

//TODO, check return value for functions which may cause error

namespace commtest {

  cliid_t comm_handler::get_one_connection(){
    if(!accept_conns) return -1;
    if(errcode) return -1;
    return clientq.pop();
  }

  int comm_handler::connect_to(std::string destip, std::string destport, cliid_t destid){
    if(errcode) return -1;    
    std::string connstr = std::string("tcp://") + destip + std::string(":") + destport;

    if(connpush.get() == NULL){
      zmq::socket_t *sock;
      try{
        sock = new zmq::socket_t(zmq_ctx, ZMQ_PUSH);
      }catch(...){
        return -1;
      }
      connpush.reset(sock);
      try{
        connpush->connect(CONN_ENDP);
      }catch(zmq::error_t &e){
        LOG(DBG, stderr, "Failed to connect to inproc socket: %s\n", e.what());
        return -1;
      }
    }

    pthread_mutex_lock(&sync_mtx);
    int ret = send_msg(*connpush, IDE2I(destid), (uint8_t *) connstr.c_str(), connstr.size(), 0);
    if(ret <= 0){
      pthread_mutex_unlock(&sync_mtx);
      return -1;
    }
    pthread_mutex_lock(&sync_mtx); //cannot return until comm thread starts running
    pthread_mutex_unlock(&sync_mtx);
    if(errcode) return -1;
    return 0;
  }

  int comm_handler::init_pub(std::string ip, std::string port, cliid_t gid, int num_subs){
    if(!ip.empty() && !port.empty()){
      std::string connstr = std::string("tcp://") + ip + std::string(":") + port;
      if(pubstart_push.get() == NULL){
        zmq::socket_t *sock;
        try{
          sock = new zmq::socket_t(zmq_ctx, ZMQ_PUSH);
        }catch(...){
          return -1;
        }
        pubstart_push.reset(sock);
        try{
          pubstart_push->connect(PUB_START_ENDP);
        }catch(zmq::error_t &e){
          LOG(DBG, stderr, "Failed to connect to inproc socket: %s\n", e.what());
          return -1;
        }
      }
      pthread_mutex_lock(&sync_mtx);
      int ret = send_msg(*pubstart_push, (uint8_t *) connstr.c_str(), connstr.size(), 0);
      if(ret <= 0){
        pthread_mutex_unlock(&sync_mtx);
        return -1;
      }
      pthread_mutex_lock(&sync_mtx); //cannot return until comm thread starts running
      pthread_mutex_unlock(&sync_mtx);
    }
    int nsubs = 0;

    if(suberq_pull.get() == NULL){
      zmq::socket_t *sock;
      try{
        sock = new zmq::socket_t(zmq_ctx, ZMQ_PULL);
      }catch(...){
        return -1;
      }
      suberq_pull.reset(sock);
      try{
        suberq_pull->connect(SUBER_Q_ENDP);
      }catch(zmq::error_t &e){
        LOG(DBG, stderr, "Failed to connect to inproc socket: %s\n", e.what());
        return -1;
      }
    }

    while(nsubs < num_subs){
      std::stringstream pubinit;
      pubinit << COMM_HANDLER_PUB_INIT;
      pubinit << "&" << id;
      int ret = publish(gid, (uint8_t *) pubinit.str().c_str(), pubinit.str().size() + 1);
      if(ret < 0) return -1;
      do{
        int cid;
        boost::shared_array<uint8_t> data;
        ret = recv_msg_async(*suberq_pull, cid, data);
        if(ret > 0){
          ++nsubs;
          LOG(DBG, stderr, "got a client: cid = %d, nsubs = %d\n", cid, nsubs);
        }
      }while(ret > 0);
      sleep(1);
    }
    return 0;
  }

  int comm_handler::subscribe_to(std::string pubip, std::string pubport, std::vector<cliid_t> pubids){
    if(errcode) return -1;
    std::string connstr;
    if(!pubip.empty() && !pubport.empty()){
      connstr = std::string("tcp://") + pubip + std::string(":") + pubport;
    }

    if(subpush.get() == NULL){
      zmq::socket_t *sock;
      try{
        sock = new zmq::socket_t(zmq_ctx, ZMQ_PUSH);
      }catch(std::bad_alloc &ba){
        return -1;
      }
      subpush.reset(sock);
      try{
        subpush->connect(SUB_PULL_ENDP);
      }catch(zmq::error_t &e){
        LOG(DBG, stderr, "Failed to connect to inproc socket: %s\n", e.what());
        return -1;
      }
    }

    std::stringstream ss;
    ss << connstr;
    ss << "&";
    int i;
    for(i = 0; i < (int) pubids.size(); ++i){
      ss << ";" << pubids[i];
    }

    pthread_mutex_lock(&sync_mtx);
    int ret = send_msg(*subpush, (uint8_t *) ss.str().c_str(), ss.str().size(), 0);
    if(ret <= 0){
      pthread_mutex_unlock(&sync_mtx);
      return -1;
    }
    pthread_mutex_lock(&sync_mtx); //cannot return until comm thread starts running
    pthread_mutex_unlock(&sync_mtx);
    if(errcode) return -1;
    return 0;
  }

  int comm_handler::publish(cliid_t group, uint8_t *data, size_t len){
    if(errcode) return -1;

    if(pub_sendpush.get() == NULL){
      zmq::socket_t *sock;
      try{
        sock = new zmq::socket_t(zmq_ctx, ZMQ_PUSH);
      }catch(std::bad_alloc &ba){
        return -1;
      }
      pub_sendpush.reset(sock);
      try{
        pub_sendpush->connect(PUB_SEND_ENDP);
      }catch(zmq::error_t &e){
        return -1;
      }
    }

    int ret = send_msg(*pub_sendpush, group, data, len, 0);
    if(errcode) return -1;
    return ret;
  }

  int comm_handler::send(cliid_t cid, uint8_t *data, size_t len){
    if(errcode) return -1;

    if(msgpush.get() == NULL){
      zmq::socket_t *sock;
      try{
        sock = new zmq::socket_t(zmq_ctx, ZMQ_PUSH);
      }catch(std::bad_alloc &ba){
        return -1;
      }
      msgpush.reset(sock);
      try{
        msgpush->connect(MSGQ_ENDP);
      }catch(zmq::error_t &e){
        return -1;
      }
    }

    int ret = send_msg(*msgpush, IDE2I(cid), data, len, 0);
    if(errcode) return -1;
    return ret;
  }

  int comm_handler::recv(cliid_t &cid, boost::shared_array<uint8_t> &data){
    if(errcode) return -1;
    LOG(DBG, stderr, "recv task!!!!!\n");    
    if(taskpull.get() == NULL){
      zmq::socket_t *sock;
      try{
        sock = new zmq::socket_t(zmq_ctx, ZMQ_PULL);
      }catch(std::bad_alloc &ba){
        return -1;
      }
      taskpull.reset(sock);
      try{
        taskpull->connect(TASKQ_ENDP);
      }catch(zmq::error_t &e){
        return -1;
      }
    }

    int incid;
    int size = recv_msg(*taskpull, incid, data);
    if(incid >= 0)
      cid = IDI2E(incid);
    else
      cid = incid;
    if(errcode) return -1;
    return size;
  }

  int comm_handler::recv_async(cliid_t &cid, boost::shared_array<uint8_t> &data){
    if(errcode) return -1;
    LOG(DBG, stderr, "recv_async task!!!!!\n");
    if(taskpull.get() == NULL){
      zmq::socket_t *sock;
      try{
        sock = new zmq::socket_t(zmq_ctx, ZMQ_PULL);
      }catch(std::bad_alloc &ba){
        return -1;
      }
      taskpull.reset(sock);
      try{
        taskpull->connect(TASKQ_ENDP);
      }catch(zmq::error_t &e){
        return -1;
      }
    }

    int incid;
    int size = recv_msg_async(*taskpull, incid, data);
    if(size == 0) return 0;
    cid = IDI2E(incid);
    if(errcode) return -1;
    return size;
  }

  //static function
  void *comm_handler::start_handler(void *_comm){
    comm_handler *comm = (comm_handler *) _comm;
    
    try{
      int sock_mandatory = 1;
      comm->router_sock.setsockopt(ZMQ_IDENTITY, &(comm->id), sizeof(cliid_t));
      comm->router_sock.setsockopt(ZMQ_ROUTER_MANDATORY, &(sock_mandatory), sizeof(int));
      LOG(DBG, stderr, "set my router sock identity to 0x%x\n", comm->id);
    }catch(zmq::error_t &e){
      LOG(DBG, stderr, "router_sock->setsockopt failed\n");
      pthread_mutex_unlock(&(comm->sync_mtx));
      return NULL;
    }

    //add more events to monitor if needed
    int ret = zmq_socket_monitor(comm->router_sock, "inproc://monitor.router_sock", ZMQ_EVENT_CONNECTED | ZMQ_EVENT_ACCEPTED | ZMQ_EVENT_DISCONNECTED);
    if(ret < 0){
      LOG(DBG, stderr, "monitor router_sock failed\n");
      comm->errcode = 1;
      pthread_mutex_unlock(&(comm->sync_mtx));
      return NULL;
    }

    try{
      comm->monitor_sock.connect("inproc://monitor.router_sock");
    }catch(zmq::error_t &e){
      LOG(DBG, stderr, "router_sock monitor connection failed\n");
      pthread_mutex_unlock(&(comm->sync_mtx));
      return NULL;
    }
    
    if(comm->accept_conns){
      std::string conn_str = std::string("tcp://") + comm->ip + std::string(":") + comm->port;
      LOG(DBG, stderr, "router_sock tries to bind to %s\n", conn_str.c_str());	
      try{
        comm->router_sock.bind(conn_str.c_str());
      }catch(zmq::error_t &e){
        LOG(DBG, stderr, "router_sock binds to %s failed: %s\n", conn_str.c_str(), e.what());
        pthread_mutex_unlock(&(comm->sync_mtx));
        return NULL;
      }
    }

    pthread_mutex_unlock(&(comm->sync_mtx));

    zmq::pollitem_t pollitems[9];
    pollitems[0].socket = comm->msgq;
    pollitems[0].events = ZMQ_POLLIN;
    pollitems[1].socket = comm->router_sock;
    pollitems[1].events = ZMQ_POLLIN;
    pollitems[2].socket = comm->shutdown_sock;
    pollitems[2].events = ZMQ_POLLIN;
    pollitems[3].socket = comm->monitor_sock;
    pollitems[3].events = ZMQ_POLLIN;
    pollitems[4].socket = comm->conn_sock;
    pollitems[4].events = ZMQ_POLLIN;
    pollitems[5].socket = comm->pub_sendpull;
    pollitems[5].events = ZMQ_POLLIN;
    pollitems[6].socket = comm->subpull;
    pollitems[6].events = ZMQ_POLLIN;
    pollitems[7].socket = comm->sub_sock;
    pollitems[7].events = ZMQ_POLLIN;
    pollitems[8].socket = comm->pubstart_pull;
    pollitems[8].events = ZMQ_POLLIN;


    while(true){
      LOG(DBG, stderr, "comm_handler starts listening...\n");

      try { 
        int num_poll;
        num_poll = zmq::poll(pollitems, 9);
        LOG(DBG, stderr, "num_poll = %d\n", num_poll);
      } catch (...) {
        comm->errcode = 1;
        LOG(NOR, stderr, "zmq::poll error!\n");
        break;
      }

      if(pollitems[0].revents){
        //LOG(DBG, stderr, "message on msgq\n");
        boost::shared_array<uint8_t> data;
        int len;
        cliid_t cid;

        len = recv_msg(comm->msgq, cid, data);
        if(len < 0){
          LOG(DBG, stderr, "recv from msgq failed");
          comm->errcode = 1;
          return NULL;
        }
        //LOG(DBG, stderr, "got message from msg queue, len = %d\n", len);
        send_msg(comm->router_sock, cid, data.get(), len, 0);
      }
      
      if(pollitems[1].revents){
        LOG(DBG, stderr, "task received on router socket!\n");
        boost::shared_array<uint8_t> data;
        int len;
        cliid_t cid;
	
        len = recv_msg(comm->router_sock, cid, data);
        if(len < 0){
          LOG(DBG, stderr, "recv from router_sock failed");
          comm->errcode = 1;
          return NULL;
        }
	
        //LOG(DBG, stderr, "received from router_sock: %s, len = %d\n", (char *) data.get(), len);
        if(len == (strlen(COMM_HANDLER_SUB_INIT) + 1)
            && memcmp(COMM_HANDLER_SUB_INIT, data.get(), strlen(COMM_HANDLER_SUB_INIT) + 1) == 0){
          //LOG(DBG, stderr, "received from router_sock --- compare successful!!\n");
          send_msg(comm->suberq, cid, data.get(), strlen(COMM_HANDLER_SUB_INIT) + 1, 0);
          pthread_mutex_unlock(&comm->sync_mtx);
        }else{
          if(comm->clientmap.count(cid) == 0){
            LOG(DBG, stderr, "this is a new client\n");
            comm->clientmap[cid] = true;
            comm->clientq.push(IDI2E(cid));
          }else if(comm->clientmap[cid] == false){
            //TODO: no connection loss detection
            LOG(DBG, stderr, "this is a reconnected client\n");
            comm->clientmap[cid] = true;
            comm->clientq.push(IDI2E(cid));
          }else{
            //LOG(DBG, stderr, "send task to taskq\n");
            send_msg(comm->taskq, cid, data.get(), len, 0);
          }
        }
      }
      
      if(pollitems[3].revents){
        zmq_event_t *event;
        boost::shared_array<uint8_t> data;
        int len;
        len = recv_msg(comm->monitor_sock, data);
	
        if (len < 0){
          LOG(DBG, stderr, "recv from monitor_sock failed");
          comm->errcode = 1;
          return NULL;
        }
        assert(len == sizeof(zmq_event_t));
        event = (zmq_event_t *) data.get();

        switch (event->event){
        case ZMQ_EVENT_CONNECTED:
          //LOG(DBG, stderr, "established connection.\n");
          break;
        case ZMQ_EVENT_ACCEPTED:
          LOG(DBG, stderr, "accepted connection.\n");
          break;
        case ZMQ_EVENT_DISCONNECTED:
          LOG(DBG, stderr, "client disconnected\n");
          break;
        default:
          LOG(DBG, stderr, "unexpected event : %d\n", event->event);
          comm->errcode = 1;
          return NULL;
        }
      }

      if(pollitems[4].revents){
	
        cliid_t destid;
        char *connstr_c;
        boost::shared_array<uint8_t> data;
        int ret = recv_msg(comm->conn_sock, destid, data);
        if(ret < 0){
          LOG(DBG, stderr, "recv from conn_sock failed!\n");
          comm->errcode = 1;
          pthread_mutex_unlock(&(comm->sync_mtx));
          return NULL;
        }
	
        connstr_c = (char *) data.get();
        try{
          comm->router_sock.connect(connstr_c);
        }catch(zmq::error_t &e){
          LOG(DBG, stderr, "Connect failed : %s\n", e.what());
          comm->errcode = 1;
          pthread_mutex_unlock(&(comm->sync_mtx));
          return NULL;
        }

        comm->clientmap[destid] = true;
        ret = -1;
        while(ret < 0){
          ret = send_msg(comm->router_sock, destid, (uint8_t *) COMM_HANDLER_INIT, strlen(COMM_HANDLER_INIT) + 1, 0);
        }
        pthread_mutex_unlock(&(comm->sync_mtx));
      }

      if(pollitems[5].revents){
        //LOG(DBG, stderr, "message on pub_sendpull\n");
        boost::shared_array<uint8_t> data;
        int len;
        cliid_t gid;

        len = recv_msg(comm->pub_sendpull, gid, data);

        if(len < 0){
          LOG(DBG, stderr, "recv from pub_sendpull failed");
          comm->errcode = 1;
          return NULL;
        }
        if(gid < 0){
          comm->errcode = 1;
          return NULL;
        }
        int ret = send_msg(comm->pub_sock, gid, data.get(), len, 0);
        if(ret < 0){
          comm->errcode = 1;
          return NULL;
        }
      }

      if(pollitems[6].revents){
        boost::shared_array<uint8_t> data;
        int len;

        len = recv_msg(comm->subpull, data);
        if(len < 0){
          LOG(DBG, stderr, "recv from subpull failed");
          comm->errcode = 1;
          return NULL;
        }
        //LOG(DBG, stderr, "received from subpull : %s\n", (char *) data.get());
        std::string dstr((char *) data.get());
        int dlim = dstr.find('&');

        std::string addr = dstr.substr(0, dlim);
        std::string gids = dstr.substr(dlim + 1, std::string::npos);

        if(!addr.empty()){
          try{
            LOG(DBG, stderr, "connect to %s\n", addr.c_str());
            comm->sub_sock.connect(addr.c_str());
          }catch(zmq::error_t &e){
            comm->errcode = 1;
            return NULL;
          }
        }

        std::stringstream ss;
        ss << gids;
        do{
          cliid_t gid;
          char temp;
          ss >> temp;
          ss >> gid;
          try{
            //LOG(DBG, stderr, "subscribe to %d\n", gid);
            comm->sub_sock.setsockopt(ZMQ_SUBSCRIBE, &gid, sizeof(cliid_t));
          }catch(zmq::error_t &e){
            comm->errcode = 1;
            return NULL;
          }
        }while(!ss.eof());
      }

      if(pollitems[7].revents){
        boost::shared_array<uint8_t> data;
        int len;
        cliid_t cid;

        len = recv_msg(comm->sub_sock, cid, data);
        if(len < 0){
          LOG(DBG, stderr, "recv from sub failed");
          comm->errcode = 1;
          return NULL;
        }

        LOG(DBG, stderr, "received from sub_sock: %s\n", (char *) data.get());

        if(memcmp(COMM_HANDLER_PUB_INIT, data.get(), strlen(COMM_HANDLER_PUB_INIT)) == 0){
          std::string dstr((char *) data.get());
          int dlim = dstr.find('&');
          std::string cidstr = dstr.substr(dlim + 1, std::string::npos);
          std::stringstream ss(cidstr);
          cliid_t cid;
          ss >> cid;
          if(comm->publishermap.count(cid) == 0){
            send_msg(comm->router_sock, cid, (uint8_t *) COMM_HANDLER_SUB_INIT, strlen(COMM_HANDLER_SUB_INIT) + 1, 0);
            comm->publishermap[cid] = true;
            pthread_mutex_unlock(&(comm->sync_mtx));
          }
        }else{
          send_msg(comm->taskq, IDE2I(cid), data.get(), len, 0);
        }
      }

      if(pollitems[8].revents){
        boost::shared_array<uint8_t> data;
        int len;

        len = recv_msg(comm->pubstart_pull, data);
        if(len < 0){
          LOG(DBG, stderr, "recv from pubstart failed");
          comm->errcode = 1;
          return NULL;
        }

        try{
          comm->pub_sock.bind((char *) data.get()) ;
        }catch(zmq::error_t &e){
          comm->errcode = 1;
          return NULL;
        }
        pthread_mutex_unlock(&(comm->sync_mtx));
      }

      if(pollitems[2].revents){
        LOG(DBG, stderr, "thread shuting down!\n");
        return NULL;
      }
    }
    return NULL;
  }
  
  int comm_handler::shutdown(){
    int msg = 1;
    zmq::socket_t s(zmq_ctx, ZMQ_PUSH);
    s.connect(SHUTDOWN_ENDP);
    int ret = send_msg(s, (uint8_t *) &msg, sizeof(int), 0);
    if(ret < 0) return -1;
    ret = pthread_join(pthr, NULL);
    return ret;
  }
}
