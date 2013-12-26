// Copyright (c) 2013, SailingLab
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


/* comm_handler.cpp
 * author: Jinliang Wei
 * */

#include <glog/logging.h>
#include <assert.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "comm_handler.hpp"
#include "zmq_util.hpp"
#include "msgtypes.hpp"
#include "petuum_ps/proxy/protocol.hpp"

namespace petuum {

  const char *KINTER_PULL_ENDP = "inproc://inter_pull_endp";
  const char *KINTER_PUSH_ENDP = "inproc://inter_push_endp";
  const char *KSUBERQ_ENDP = "inproc://suberq_endp";
  const char *KROUTER_MONITOR_ENDP = "inproc://monitor.router_sock";
  const char *KTHR_RECV_ENDP_PREFIX = "inproc://thr_recv"; // append ":" 
                                                          // + thrid for addr
  const char *KTHR_KEY_RECV_ENDP_PREFIX = "inproc://thr_key_recv"; 
  // append ":" + thrid + key for addr


  namespace {
    inline int InitSockIfHaveNot(boost::thread_specific_ptr<zmq::socket_t> *_sock,
				 zmq::context_t *_zmq_ctx, int _type,
				 const char *_connect_endp){
      if(_sock->get() == NULL){
        try{
	  // zmq::socket_t() may throw error_t
          VLOG(2) << "will create new zmq::sock_t, type = " <<_type
		    << " connect_endp = " << _connect_endp;
	  _sock->reset(new zmq::socket_t(*_zmq_ctx, _type));
          VLOG(2) << "create new zmq::sock_t done, type = " << _type
		    << " connect_endp = " << _connect_endp;

        }catch(...){
          return -1;
        }
        try{
	  VLOG(2) << "start connecting to " << _connect_endp;
          (*_sock)->connect(_connect_endp);
	  VLOG(2) << "done connecting to " << _connect_endp;

        }catch(zmq::error_t &e){
	  LOG(ERROR) << "connect failed, e.what() = " << e.what();
          return -1;
       }
      }
      return 0;
    }
  }  // anonymous namespace

  /*
   * To avoid failure of constructor, which will throw exception, we have to
   * avoid initializations that may fail. This is why we use pointers for
   * zmq_ctx and sockets.
   * */
  CommHandler::CommHandler(const ConfigParam &_cparam):
      zmq_ctx_(NULL),
      owns_zmq_ctx_(false),
      inter_pull_sock_(NULL),
      inter_push_sock_(NULL),
      router_sock_(NULL),
      monitor_sock_(NULL),
      pub_sock_(NULL),
      sub_sock_(NULL),
      pthr_(),
      id_(_cparam.id_),
      ip_(_cparam.ip_),
      port_(_cparam.port_),
      errcode_(0),
      accept_conns_(_cparam.accept_conns_),
      pub_sock_created_(false),
      comm_started_(false) { }

  CommHandler::~CommHandler(){
    VLOG(1) << "CommHandler is being destroyed...";
    pthread_mutex_destroy(&sync_mtx_);
    if (owns_zmq_ctx_) {
      delete zmq_ctx_;
    }
    VLOG(1) << "CommHandler is destroyed.";
    // TODO(jinliang): consider deleting pointers in comm_thr_push_sock_
    // and comm_thr_key_push_sock_. It is not an error to not delete them
    // as user threads are suppose to deregister. However, we may delete them
    // to make the life of users a little easier in certain situations.
  }

  int CommHandler::Init(zmq::context_t *_zmq_ctx){

    if(comm_started_) return -1;

    if(_zmq_ctx == 0) {
      try{
        zmq_ctx_ = new zmq::context_t(1);
      }catch(...){ //zmq exception and bad_alloc exception
        errcode_ = 1;
        return -1;
      }
      // delete zmq_ctx_ in ~CommHandler()
      owns_zmq_ctx_ = true;
    } else {
      zmq_ctx_ = _zmq_ctx;
    }

    pthread_mutex_init(&sync_mtx_, NULL);
    pthread_cond_init(&sync_cond_, NULL);

    // cond variable should be initialized before comm thread may touch it
    pthread_mutex_lock(&sync_mtx_);
    cond_comm_thr_created_ = false;
    pthread_mutex_unlock(&sync_mtx_);
    
    int ret = pthread_create(&pthr_, NULL, StartHandler, this);
    if(ret != 0) {
      return -1;
    }
  
    pthread_mutex_lock(&sync_mtx_);
    while(!cond_comm_thr_created_){
      pthread_cond_wait(&sync_cond_, &sync_mtx_);
    }
    pthread_mutex_unlock(&sync_mtx_);

    comm_started_ = true;
    if(errcode_) {
      LOG(ERROR) << "Init errcode_ = " << errcode_;
      return -1;
    }
    return 0;
  }

  /* The blocking effect on connection-init functions is achieved by condition
   * variable.
   *
   *  ConnectTo(), InitPub(), SubscribeTo() can be invoked more than once but
   *  they are not thread-safe.
   *
   *  The main reason is that pthread specification does not guarantee
   *  threads that wait on condition variable are signaled in order.
   * */

  int32_t CommHandler::GetOneConnection(){
    if(!accept_conns_) return -1;
    VLOG(1) << "GetOneConnection errcode_ = " << errcode_;
    if(errcode_ || !comm_started_) return -1;
    return clientq_.Pop();
  }

  int CommHandler::ConnectTo(std::string _destip, std::string _destport,
                             int32_t _destid){
    if(errcode_ || !comm_started_) return -1;
    VLOG(1) << "ConnectTo here, ip = " << _destip 
	      << " port = " << _destport
	      << " id = " << _destid
	      << std::endl;
    int ret = InitSockIfHaveNot(&client_push_sock_, zmq_ctx_, ZMQ_PUSH, 
				KINTER_PULL_ENDP);
    if(ret < 0){
      errcode_ = 1;
      return -1;
    }
    CommConnectMsg msg;
    msg.cid_ = _destid;
    strcpy(msg.ip_, _destip.c_str());
    strcpy(msg.port_, _destport.c_str());

    pthread_mutex_lock(&sync_mtx_);
    cond_connection_done_ = false;
    pthread_mutex_unlock(&sync_mtx_);
 
    ret = SendMsg(*client_push_sock_, (uint8_t *) &msg, sizeof(CommConnectMsg), 0);
    if(ret <= 0){
      errcode_ = 1;
      return -1;
    }
    
    pthread_mutex_lock(&sync_mtx_);
    while(!cond_connection_done_){
      pthread_cond_wait(&sync_cond_, &sync_mtx_);
    }
    // cannot return until the router thread is really connected to remote thread
    pthread_mutex_unlock(&sync_mtx_);
    if(errcode_) return -1;
    return 0;
  }

  int CommHandler::InitPub(std::string _ip, std::string _port, int32_t _gid,
                           int _num_subs){
    if(errcode_ || !comm_started_) return -1;

    /*
     * In most cases, InitPub() is called only once and all accesses to the
     * pub_sock_ will happen after InitPub(). In that case, we can let InitPub()
     *  directly access the pub_sock_ to bind it and set its options. That
     * approach is better in terms of performance.
     * Here I (Jinliang) still package the data and send it to comm thread to
     * it do the work, because
     * 1) Since InitPub() is only called when initializing publish, losing a
     * little bit performance on it is fine;
     * 2) There might be special usage that clients want to dynamically set up a
     * publish group while some publish groups have already been created and
     * actively used. We'd like to support that.
     * 3) To simplify the code and make it less error-prone, we have the rule
     * requires pub_sock_ to be only accessed by the comm thread. We do not want
     *  to make an exception since we are not gaining much performance on that.
     * */

    int ret = InitSockIfHaveNot(&client_push_sock_, zmq_ctx_, ZMQ_PUSH, 
				KINTER_PULL_ENDP);
    if(ret < 0){
      errcode_ = 1;
      return -1;
    }
      

    /* If user does not pass in any IP or port, we assume that they just want to
     *  set up another publish group.
     * */
    bool pub_init = (_ip.empty()) ? false : true;
    if(pub_init && !pub_sock_created_){
      CommPubInitMsg msg;
      strcpy(msg.ip_, _ip.c_str());
      strcpy(msg.port_, _port.c_str());

      pthread_mutex_lock(&sync_mtx_);
      cond_pub_init_done_ = false;
      pthread_mutex_unlock(&sync_mtx_);

      ret = SendMsg(*client_push_sock_, (uint8_t *) &msg, sizeof(CommPubInitMsg), 0);
      if(ret <= 0){
        errcode_ = 1;
        return -1;
      }      
      
      pthread_mutex_lock(&sync_mtx_);
      while(!cond_pub_init_done_){
	pthread_cond_wait(&sync_cond_, &sync_mtx_);
      }
      pthread_mutex_unlock(&sync_mtx_);
      
      if(errcode_) return -1;
      pub_sock_created_ = true;
    }

    zmq::socket_t suberq_pull(*zmq_ctx_, ZMQ_PULL);

    try{
      suberq_pull.connect(KSUBERQ_ENDP);
    }catch(...){
      errcode_ = 1;
      return -1;
    }

    /*
     * To know how many clients subscribe to this publish group, we publish a
     * message and let all clients that have seen the message but not yet
     * replied to reply to this message.
     * Note that the published message might not be received by any clients as
     * they might have not yet subscribed, so we need to send the message
     * multiple times.
     * What about let the comm thread do all this logic? That would make code
     * too complicated.
     * 1) We need to publish the init message multiple times. So we need to have
     *  a loop in the "CommPubInit" branch.
     * 2) The comm thread need also keep checking the router socket for incoming
     *  messages. Incoming messages are not just RouterPubInitReply, they could
     *  be any other kind of Router messages, so the complete logic for checking
     *   message type should be repeated again.
     * */
    VLOG(1) << "send PubInitSend msg";
    int nsubs = 0;
    while(nsubs < _num_subs){
      CommPubInitSendMsg msg;
      msg.gid_ = _gid;
      int ret = SendMsg(*client_push_sock_, (uint8_t *) &msg,
          sizeof(CommPubInitSendMsg), 0);
      if(ret < 0) return -1;
      sleep(1);
      do{
        boost::shared_array<uint8_t> data;
        ret = RecvMsgAsync(suberq_pull, data); // We have to use async recv
                                               // here because there might
                                               // be no message to receive
        if(ret > 0){
          ++nsubs;
          VLOG(2) << "nsubs = " << nsubs << std::endl;
        }
      }while(ret > 0 &&(nsubs < _num_subs));
    }
    return 0;
  }

  int CommHandler::SubscribeTo(std::string _pubip, std::string _pubport,
                                std::vector<int32_t> _pubids){
    if(errcode_ || !comm_started_) return -1;
    int ret = InitSockIfHaveNot(&client_push_sock_, zmq_ctx_, ZMQ_PUSH, 
				KINTER_PULL_ENDP);
    if(ret < 0){
      errcode_ = 1;
      return -1;
    }
    CommSubInitMsg msg;
    strcpy(msg.ip_, _pubip.c_str());
    strcpy(msg.port_, _pubport.c_str());
    msg.num_gids_ = _pubids.size();

    pthread_mutex_lock(&sync_mtx_);
    cond_subscribe_done_ = false;
    pthread_mutex_unlock(&sync_mtx_);
    // first send out the number of publish groups to subscribe to
    // then send out the ids of the publish groups separately
    ret = SendMsg(*client_push_sock_, (uint8_t *) &msg,
        sizeof(CommSubInitMsg), ZMQ_SNDMORE);
    if(ret <= 0){
      errcode_ = -1;
      return -1;
    }
    int snd_more = ZMQ_SNDMORE;
    for(int i = 0; i < msg.num_gids_; ++i){
      if(i == (msg.num_gids_ - 1)) snd_more = 0;
      ret = SendMsg(*client_push_sock_, (uint8_t *) &(_pubids[i]),
          sizeof(int32_t), snd_more);
      if(ret <= 0){
        errcode_ = 1;
        return -1;
      }
    }
    
    pthread_mutex_lock(&sync_mtx_);
    while(!cond_subscribe_done_){
      pthread_cond_wait(&sync_cond_, &sync_mtx_);
    }
    pthread_mutex_unlock(&sync_mtx_);
    if(errcode_) return -1;
    return 0;
  }

  int CommHandler::Publish(int32_t _group, uint8_t *_data, size_t _len, bool
      _more, bool _first_part){
    if(errcode_ || !comm_started_) return -1;
    int ret;
    if(_first_part){
      int ret = InitSockIfHaveNot(&client_push_sock_, zmq_ctx_, ZMQ_PUSH, 
				  KINTER_PULL_ENDP);

      if(ret < 0){
	errcode_ = 1;
	return -1;
      }

      CommPublishMsg msg;
      msg.gid_ = _group;

      ret = SendMsg(*client_push_sock_, (uint8_t *) &msg, sizeof(CommPublishMsg),
          ZMQ_SNDMORE);
      if(ret <= 0){
        errcode_ = 1;
        return -1;
      }
    }
    ret = SendMsg(*client_push_sock_, _data, _len, (_more ? ZMQ_SNDMORE : 0));
    if(ret <= 0){
      errcode_ = 1;
      return -1;
    }
    if(errcode_) return -1;
    return ret;
  }

  int CommHandler::Send(int32_t _cid, const uint8_t *_data, size_t _len,
      bool _more, bool _first_part){
    VLOG(3) << "Send called";
    if(errcode_ || !comm_started_) return -1;
    int ret;
    if(_first_part){
      int ret = InitSockIfHaveNot(&client_push_sock_, zmq_ctx_, ZMQ_PUSH, 
				  KINTER_PULL_ENDP);
      if(ret < 0){
	errcode_ = 1;
	return -1;
      }
      CommSendMsg msg;
      msg.cid_ = _cid;
      VLOG(2) << "Send msg to " << _cid;
      ret = SendMsg(*client_push_sock_, (uint8_t *) &msg, sizeof(CommSendMsg),
          ZMQ_SNDMORE);
      if(ret <= 0){
        errcode_ = 1;
        return -1;
      }
    }

    ret = SendMsg(*client_push_sock_, _data, _len, (_more ? ZMQ_SNDMORE : 0));
    if(ret <= 0){
      errcode_ = 1;
      return -1;
    }

    if(errcode_) return -1;
    return ret;
  }

  int CommHandler::Recv(int32_t &_cid, boost::shared_array<uint8_t> &_data, 
			bool *_more){
    VLOG(3) << "********Recv called";
    if(errcode_ || !comm_started_){
      LOG(FATAL) << "errcode_ = " << errcode_
		 << " comm_started = " << comm_started_;
      return -1;
    }
    if(client_pull_sock_.get() == NULL){
      LOG(FATAL) << "thread not registered";
      return -1;
    }
    int size = RecvMsg(*client_pull_sock_, _cid, _data, _more);
    if(errcode_) return -1;
    LOG_IF(ERROR, size <= 0) << "size = " << size;
    return size;
  }

  int CommHandler::Recv(boost::shared_array<uint8_t> &_data, bool *_more){
    if(errcode_ || !comm_started_) return -1;
    if(client_pull_sock_.get() == NULL) return -1;

    int size = RecvMsg(*client_pull_sock_, _data, _more);
    if(errcode_) return -1;
    return size;
  }

  int CommHandler::RecvAsync(int32_t &_cid, boost::shared_array<uint8_t> &_data,
      bool *_more){
    if(errcode_ || !comm_started_) return -1;
    if(client_pull_sock_.get() == NULL) return -1;

    int size = RecvMsgAsync(*client_pull_sock_, _cid, _data, _more);
    if(size == 0) return 0;
    if(errcode_) return -1;
    return size;
  }

  int CommHandler::RecvAsync(boost::shared_array<uint8_t> &_data, bool *_more){
    if(errcode_ || !comm_started_) return -1;
    if(client_pull_sock_.get() == NULL) return -1;

    int size = RecvMsgAsync(*client_pull_sock_, _data, _more);
    if(size == 0) return 0;
    if(errcode_) return -1;
    return size;
  }

  int CommHandler::ShutDown(){
    if(!comm_started_) return -1;
    EMsgType msg_type = CommShutDown;
    int ret = InitSockIfHaveNot(&client_push_sock_, zmq_ctx_, ZMQ_PUSH, 
				KINTER_PULL_ENDP);
    if(ret < 0) return -1;
    ret = SendMsg(*client_push_sock_, (uint8_t *) &msg_type, 
		      sizeof(EMsgType), 0);
    if(ret < 0) return -1;
    ret = pthread_join(pthr_, NULL);
    comm_started_ = false;
    VLOG(0) << "CommHandler ShutDown successfully";
    return ret;
  }

  /*======= Thread-specific APIs ==========================================*/

  int CommHandler::RegisterThr(int32_t _type){
    if(errcode_ || !comm_started_){
      LOG(ERROR) << "errocde_ = " << errcode_ << " comm_started_ = " 
		<< comm_started_; 

      return -1;
    }

    if(_type && RegThr_Thr){
      if(client_thr_pull_sock_.get() != NULL) return -1; // has registered
    
      int ret = InitSockIfHaveNot(&client_push_sock_, zmq_ctx_, ZMQ_PUSH, 
				  KINTER_PULL_ENDP);
      if(ret < 0) return -1;

      CommThrRegMsg msg;
      msg.thrid_ = pthread_self();
      
      pthread_mutex_lock(&sync_mtx_);
      cond_register_thr_done_ = false;
      pthread_mutex_unlock(&sync_mtx_);
      
      ret = SendMsg(*client_push_sock_, (uint8_t *) &msg, 
		    sizeof(CommThrRegMsg), 0);
      if(ret < 0) return -1;
      
      pthread_mutex_lock(&sync_mtx_);
      while(!cond_register_thr_done_){
	pthread_cond_wait(&sync_cond_, &sync_mtx_);
      }
      pthread_mutex_unlock(&sync_mtx_);

      if(errcode_) return -1;
      
      std::string conn_end(KTHR_RECV_ENDP_PREFIX);
      conn_end = conn_end + std::string(":") + std::to_string(msg.thrid_);

      ret = InitSockIfHaveNot(&client_thr_pull_sock_, zmq_ctx_, ZMQ_PULL, 
			      conn_end.c_str());
      if(ret < 0){
	errcode_ = 1;
	return -1;
      }
    }

    if(_type && RegThr_Recv){
      if(client_pull_sock_.get() != NULL) return -1;
      int ret = InitSockIfHaveNot(&client_pull_sock_, zmq_ctx_, ZMQ_PULL, 
				  KINTER_PUSH_ENDP);
      if(ret < 0){
	errcode_ = 1;
	return -1;
      }
    }
    return 0;
  }

  int CommHandler::DeregisterThr(int32_t _type){
    if(errcode_ || !comm_started_) return -1;
    if(client_thr_pull_sock_.get() == NULL) return -1; // has registered
    
    if(_type && RegThr_Thr){
      CommThrDeregMsg msg;
      msg.thrid_ = pthread_self();
      
      client_thr_pull_sock_.reset(); // shut down socket on my end

      pthread_mutex_lock(&sync_mtx_);
      cond_deregister_thr_done_ = false;
      pthread_mutex_unlock(&sync_mtx_);

      int ret = SendMsg(*client_push_sock_, (uint8_t *) &msg, 
			sizeof(CommThrDeregMsg), 0);
      if(ret < 0) return -1;
      

      pthread_mutex_lock(&sync_mtx_);
      while(!cond_deregister_thr_done_){
	pthread_cond_wait(&sync_cond_, &sync_mtx_);
      }
      pthread_mutex_unlock(&sync_mtx_);
      
      if(errcode_) return -1;
    }

    if(_type && RegThr_Recv){
      client_pull_sock_.reset();
    }
    return 0;
  }

  int CommHandler::SendThr(int32_t _cid, pthread_t _dst_thrid, 
			   const uint8_t *_data, int32_t _len, bool _more, 
			   bool _first_part){
    if(errcode_ || !comm_started_) return -1;

    int ret;
    if(_first_part){
      int ret = InitSockIfHaveNot(&client_push_sock_, zmq_ctx_, ZMQ_PUSH, 
				  KINTER_PULL_ENDP);
      if(ret < 0){
	errcode_ = 1;
	return -1;
      }

      CommThrSendMsg msg;
      msg.cid_ = _cid;
      msg.thrid_ = _dst_thrid;

      ret = SendMsg(*client_push_sock_, (uint8_t *) &msg, sizeof(CommThrSendMsg),
          ZMQ_SNDMORE);
      if(ret <= 0){
        errcode_ = 1;
        return -1;
      }
    }

    ret = SendMsg(*client_push_sock_, _data, _len, (_more ? ZMQ_SNDMORE : 0));
    if(ret <= 0){
      errcode_ = 1;
      return -1;
    }
    VLOG(3) << "SendThr done";
    if(errcode_) return -1;
    return ret;
  }

  int CommHandler::RecvThr(int32_t &_cid, boost::shared_array<uint8_t> &_data, 
			   bool *_more){
    
    if(errcode_ || !comm_started_){
      LOG(ERROR) << "internal error has happened to comm_handler";
      return -1;
    }    
    if(client_thr_pull_sock_.get() == NULL){
      LOG(ERROR) << "thread has not yet registered";
      return -1;
    }
    int size = RecvMsg(*client_thr_pull_sock_, _cid, _data, _more);
    if(errcode_){
      LOG(ERROR) << "RecvThr, errcode_ = " << errcode_;
      return -1;
    }
    VLOG(2) << "RecvThr, size = " << size;
    return size;
  }

  int CommHandler::RecvThr(boost::shared_array<uint8_t> &_data, 
			   bool *_more){
    if(errcode_ || !comm_started_) return -1;
    if(client_thr_pull_sock_.get() == NULL) return -1;
    int size = RecvMsg(*client_thr_pull_sock_, _data, _more);
    
    if(errcode_) return -1;
    return size;
  }

  int CommHandler::RecvThrAsync(int32_t &_cid, boost::shared_array<uint8_t> &_data, 
				bool *_more){
    if(errcode_ || !comm_started_) return -1;
    if(client_thr_pull_sock_.get() == NULL) return -1;
    int size = RecvMsgAsync(*client_thr_pull_sock_, _cid, _data, _more);
    if(errcode_) return -1;
    return size;
  }

  int CommHandler::RecvThrAsync(boost::shared_array<uint8_t> &_data, 
				bool *_more){
    if(errcode_ || !comm_started_) return -1;
    if(client_thr_pull_sock_.get() == NULL) return -1;
    int size = RecvMsgAsync(*client_thr_pull_sock_, _data, _more);
    if(errcode_) return -1;
    return size;
  }

  /*======================= Thread-specific APIs end ====================*/

  /*=================== Thread-specific Keyed messages APIs ==============*/

  int CommHandler::RegisterThrKey(int32_t _key){
    VLOG(1) << "RegisterThrKey starts";

    if(errcode_ || !comm_started_) return -1;

    if(client_thr_key_pull_sock_.get() == NULL){
      try{
	VLOG(1) << "Try creating key-socket map";
	client_thr_key_pull_sock_.reset(new 
			      boost::unordered_map<int32_t, zmq::socket_t *>());
      }catch(std::bad_alloc &ba){
	return -1;
      }    
    }
    VLOG(1) << "Created key-socket map";

    if((*client_thr_key_pull_sock_)[_key] != NULL) return -1; // this key has 
                                                              // been registered

    int ret = InitSockIfHaveNot(&client_push_sock_, zmq_ctx_, ZMQ_PUSH, 
				KINTER_PULL_ENDP);
    if(ret < 0) return -1;

    CommThrKeyRegMsg msg;
    msg.thrid_ = pthread_self();
    msg.key_ = _key;

    pthread_mutex_lock(&sync_mtx_);
    cond_register_thr_key_done_ = false;
    pthread_mutex_unlock(&sync_mtx_);

    ret = SendMsg(*client_push_sock_, (uint8_t *) &msg, 
		      sizeof(CommThrKeyRegMsg), 0);
    if(ret < 0) return -1;

    pthread_mutex_lock(&sync_mtx_);
    while(!cond_register_thr_key_done_){
      pthread_cond_wait(&sync_cond_, &sync_mtx_);
    }
    pthread_mutex_unlock(&sync_mtx_);

    if(errcode_) return -1;

    std::string conn_end(KTHR_KEY_RECV_ENDP_PREFIX);
    conn_end = conn_end + ":" + std::to_string(msg.thrid_) + ":" 
      + std::to_string(_key);
    
    zmq::socket_t *thr_key_pull_sock;
    try{
      thr_key_pull_sock = new zmq::socket_t(*zmq_ctx_, ZMQ_PULL);
    }catch(...){
      LOG(ERROR) << "create new socket failed";
      return -1;
    }
    try{
      thr_key_pull_sock->connect(conn_end.c_str());
    }catch(zmq::error_t &e){
      LOG(ERROR) << "create new socket failed";
      return -1;
    }

    (*client_thr_key_pull_sock_)[_key] = thr_key_pull_sock;
    return 0;
  }

  int CommHandler::DeregisterThrKey(int32_t _key){
    if(errcode_ || !comm_started_) return -1;
    if((*client_thr_key_pull_sock_)[_key] == NULL) return -1; // this key has 
                                                              // been registered
    
    CommThrKeyDeregMsg msg;
    msg.thrid_ = pthread_self();
    msg.key_ = _key;

    delete (*client_thr_key_pull_sock_)[_key]; // shut down socket on my end
    client_thr_key_pull_sock_->erase(_key);


    pthread_mutex_lock(&sync_mtx_);
    cond_deregister_thr_key_done_ = false;
    pthread_mutex_unlock(&sync_mtx_);

    int ret = SendMsg(*client_push_sock_, (uint8_t *) &msg, 
		      sizeof(CommThrKeyDeregMsg), 0);
    if(ret < 0) return -1;


    pthread_mutex_lock(&sync_mtx_);
    while(!cond_deregister_thr_key_done_){
      pthread_cond_wait(&sync_cond_, &sync_mtx_);
    }
    pthread_mutex_unlock(&sync_mtx_);

    if(errcode_) return -1;

    return 0;
  }

  int CommHandler::SendThrKey(int32_t _cid, pthread_t _dst_thrid, int32_t _key, 
			   const uint8_t *_data, int32_t _len, bool _more, 
			   bool _first_part){
    if(errcode_ || !comm_started_) return -1;
    
    int ret;
    if(_first_part){
      int ret = InitSockIfHaveNot(&client_push_sock_, zmq_ctx_, ZMQ_PUSH, 
				  KINTER_PULL_ENDP);
      if(ret < 0){
	errcode_ = 1;
	return -1;
      }

      CommThrKeySendMsg msg;
      msg.cid_ = _cid;
      msg.thrid_ = _dst_thrid;
      msg.key_ = _key;

      ret = SendMsg(*client_push_sock_, (uint8_t *) &msg, sizeof(CommThrKeySendMsg),
          ZMQ_SNDMORE);
      if(ret <= 0){
        errcode_ = 1;
        return -1;
      }
    }

    ret = SendMsg(*client_push_sock_, _data, _len, (_more ? ZMQ_SNDMORE : 0));
    if(ret <= 0){
      errcode_ = 1;
      return -1;
    }
    
    if(errcode_) return -1;
    return ret;
  }

  int CommHandler::RecvThrKey(int32_t &_cid, int32_t _key, 
			      boost::shared_array<uint8_t> &_data, bool *_more){
    
    VLOG(3) << "RecvThrKey() called";
    if(errcode_ || !comm_started_) return -1;
    if(client_thr_key_pull_sock_.get() == NULL) return -1;
    
    if((*client_thr_key_pull_sock_)[_key] == NULL) {
      LOG(ERROR) << "not yet registered for key " << _key;
      return -1;
    }  
    VLOG(3) << "RecvThrKey() calling RecvMsg()";

    int size = RecvMsg(*((*client_thr_key_pull_sock_)[_key]),
		       _cid, _data, _more);
    VLOG(0) << "RecvThrKey size = " << size;
    if(errcode_) return -1;
    return size;
  }

  int CommHandler::RecvThrKey(int32_t _key, boost::shared_array<uint8_t> &_data,
			      bool *_more){
    if(errcode_ || !comm_started_) return -1;
    if(client_thr_key_pull_sock_.get() == NULL) return -1;
    if((*client_thr_key_pull_sock_)[_key] == NULL) return -1;
    
    int size = RecvMsg(*((*client_thr_key_pull_sock_)[_key]),
		       _data, _more);
    if(errcode_) return -1;
    return size;
  }

  int CommHandler::RecvThrKeyAsync(int32_t &_cid, int32_t _key, 
				   boost::shared_array<uint8_t> &_data, 
				   bool *_more){
    if(errcode_ || !comm_started_) return -1;
    if(client_thr_key_pull_sock_.get() == NULL) return -1;
    if((*client_thr_key_pull_sock_)[_key] == NULL) return -1;
    
    int size = RecvMsgAsync(*((*client_thr_key_pull_sock_)[_key]),
			    _cid, _data, _more);
    if(errcode_) return -1;
    return size;
  }

  int CommHandler::RecvThrKeyAsync(int32_t _key, 
				   boost::shared_array<uint8_t> &_data, 
				   bool *_more){
    if(errcode_ || !comm_started_) return -1;
    
    if(client_thr_key_pull_sock_.get() == NULL) return -1;
    if((*client_thr_key_pull_sock_)[_key] == NULL) return -1;
    
    int size = RecvMsgAsync(*((*client_thr_key_pull_sock_)[_key]),
		       _data, _more);
    if(errcode_) return -1;
    return size;
  }

  /*========= Private functions =========================================*/

  // static function
  void *CommHandler::StartHandler(void *_comm){
    // This is the function that the communication thread runs.
    // The communication thread is responsible for
    // 1) translates client id: the client ids are used in the above interface
    //    functions are all user-readable ids, but they cannot be used by 0MQ
    //    directly because 0MQ requires the last byte of the id to be non-zero
    // 2) parse CommXXX types and message format, construct RouterXXX messages

    CommHandler *comm = (CommHandler *) _comm;

    try{
      comm->inter_pull_sock_.reset(new zmq::socket_t(*(comm->zmq_ctx_), ZMQ_PULL));
      comm->inter_push_sock_.reset(new zmq::socket_t(*(comm->zmq_ctx_), ZMQ_PUSH));
      comm->router_sock_.reset(new zmq::socket_t(*(comm->zmq_ctx_), ZMQ_ROUTER));
      comm->sub_sock_.reset(new zmq::socket_t(*(comm->zmq_ctx_), ZMQ_SUB));
      comm->monitor_sock_.reset(new zmq::socket_t(*(comm->zmq_ctx_), ZMQ_PAIR));
    }catch(...){
      comm->errcode_ = 1;
      LOG(ERROR) << "failed to construct sockets" << std::endl;
      return NULL;
    }

   try{
      comm->inter_pull_sock_->bind(KINTER_PULL_ENDP);
      comm->inter_push_sock_->bind(KINTER_PUSH_ENDP);
    }catch(...){
      LOG(ERROR) << "failed to bind inproc sockets" << std::endl;
      comm->errcode_ = 1;
      return NULL;
    }

    // router socket initialization
    try{
      int sock_mandatory = 1;
      int32_t my_id = CidToZmqRid(comm->id_);
      comm->router_sock_->setsockopt(ZMQ_IDENTITY, &my_id, sizeof(int32_t));
      comm->router_sock_->setsockopt(ZMQ_ROUTER_MANDATORY, &(sock_mandatory),
                                     sizeof(int));
      int buf_size = 0x7FFFFFFF;
      comm->router_sock_->setsockopt(ZMQ_SNDBUF, &buf_size, sizeof(int));
      comm->router_sock_->setsockopt(ZMQ_RCVBUF, &buf_size, sizeof(int));

    }catch(zmq::error_t &e){
      comm->errcode_ = 1;

      pthread_mutex_lock(&(comm->sync_mtx_));
      comm->cond_comm_thr_created_ = true;
      pthread_cond_signal(&(comm->sync_cond_));
      pthread_mutex_unlock(&(comm->sync_mtx_));

      LOG(ERROR) << "set up router_sock_ failed: num = " << e.num() << " what: "
          << e.what() << std::endl;
      return NULL;
    }

    //add more events to monitor if needed
    int ret = zmq_socket_monitor(*comm->router_sock_, KROUTER_MONITOR_ENDP,
          ZMQ_EVENT_CONNECTED | ZMQ_EVENT_ACCEPTED | ZMQ_EVENT_DISCONNECTED);
    if(ret < 0){
      comm->errcode_ = 1;
      LOG(ERROR) << "set up monitor_sock_ failed" << std::endl;

      pthread_mutex_lock(&(comm->sync_mtx_));
      comm->cond_comm_thr_created_ = true;
      pthread_cond_signal(&(comm->sync_cond_));
      pthread_mutex_unlock(&(comm->sync_mtx_));

      return NULL;
    }

    try{
      comm->monitor_sock_->connect(KROUTER_MONITOR_ENDP);
    }catch(zmq::error_t &e){
      LOG(ERROR) << "monitor_sock_ connect failed" << std::endl;
      comm->errcode_ = 1;

      pthread_mutex_lock(&(comm->sync_mtx_));
      comm->cond_comm_thr_created_ = true;
      pthread_cond_signal(&(comm->sync_cond_));
      pthread_mutex_unlock(&(comm->sync_mtx_));

      return NULL;
    }
    
    if(comm->accept_conns_){
      std::string conn_str = std::string("tcp://") + comm->ip_
              + std::string(":") + comm->port_;
      try{
        comm->router_sock_->bind(conn_str.c_str());
      }catch(zmq::error_t &e){
        comm->errcode_ = 1;
        LOG(ERROR) << "router_sock_ bind failed" << std::endl;

        pthread_mutex_lock(&(comm->sync_mtx_));
	comm->cond_comm_thr_created_ = true;      
        pthread_cond_signal(&(comm->sync_cond_));
        pthread_mutex_unlock(&(comm->sync_mtx_));

        return NULL;
      }
    }

    // The communication thread has completed the initialization for router
    // socket, Init() may return
    pthread_mutex_lock(&(comm->sync_mtx_));
    comm->cond_comm_thr_created_ = true;
    pthread_cond_signal(&(comm->sync_cond_));
    pthread_mutex_unlock(&(comm->sync_mtx_));
    //VLOG(0) << "comm thread init succeeded" << std::endl;

    zmq::pollitem_t pollitems[4];
    pollitems[0].socket = *(comm->inter_pull_sock_.get());
    pollitems[0].events = ZMQ_POLLIN;
    pollitems[1].socket = *(comm->router_sock_.get());
    pollitems[1].events = ZMQ_POLLIN;
    pollitems[2].socket = *(comm->monitor_sock_.get());
    pollitems[2].events = ZMQ_POLLIN;
    pollitems[3].socket = *(comm->sub_sock_.get());
    pollitems[3].events = ZMQ_POLLIN;

    while(true){
      try { 
        int num_poll;
        num_poll = zmq::poll(pollitems, 4);
        VLOG(2) << "poll get " << num_poll << " messages" << std::endl;
      } catch (zmq::error_t &e) {
        comm->errcode_ = 1;
        LOG(FATAL) << "comm thread pull failed, error = " << e.what() << "\nPROCESS EXIT";
        // if I fail to poll, I don't know what is being sent to me
        // the client thread might be waiting on condtion variable -- just exit
        //exit(1);
      }

      /* inter_pull_sock_ events START */
      if(pollitems[0].revents){
        VLOG(2) << "inter_pull_sock_ gets a message!" << std::endl;
        boost::shared_array<uint8_t> data;
        int len;
        EMsgType comm_type;
        len = RecvMsg(*comm->inter_pull_sock_, data);
        if(len < 0){
          comm->errcode_ = 1;
          LOG(FATAL) << "comm thread read message type failed, "
                     << "error \nPROCESS EXIT!";
          // if I fail to poll, I don't know what is being sent to me
          // the client thread might be waiting on condtion variable -- just exit
          exit(1);
        }

        comm_type = *((EMsgType *) data.get());

        switch(comm_type){
        case CommConnect:
        {
          CommConnectMsg *msg = (CommConnectMsg *) data.get();
          int32_t cid = msg->cid_;
          std::string connect_ip(msg->ip_);
          std::string connect_port(msg->port_);

          // start connecting
          std::string conn_str = std::string("tcp://") + connect_ip
                              + std::string(":") + connect_port;
          try{
            comm->router_sock_->connect(conn_str.c_str());
          }catch(zmq::error_t &e){
            comm->errcode_ = 1;
            pthread_mutex_lock(&(comm->sync_mtx_));
	    comm->cond_connection_done_ = true;
	    pthread_cond_signal(&(comm->sync_cond_));
            pthread_mutex_unlock(&(comm->sync_mtx_));
            return NULL;
          }

          // the destination id is added to the client map
          comm->clientmap_[cid] = true;
          ret = -1;

          EMsgType type = RouterConnect;
          // When the client is firstly connected, there's an undetermined
          // period of time when send will fail (0MQ's property), so we conduct
          //  a successful send before return to user to guarantee that users
          // can successfully send after connect succeeds.
          // Also, when a client is successfully connected, the other end does
          // not know the client's ID, so we have to send a message over to let
          //  the other end konw the connected client.
          while(ret < 0){
            //VLOG(0) << "dest id = " << CidToZmqRid(cid) << std::endl;
            ret = SendMsg(*comm->router_sock_, CidToZmqRid(cid), (uint8_t *) &type,
                sizeof(EMsgType), 0);
          }

          pthread_mutex_lock(&(comm->sync_mtx_));
	  comm->cond_connection_done_ = true;
          pthread_cond_signal(&(comm->sync_cond_));
          pthread_mutex_unlock(&(comm->sync_mtx_));

          break;
        }
        case CommPubInit:
        {
          // create pub_sock_
          comm->pub_sock_.reset(new zmq::socket_t(*(comm->zmq_ctx_), ZMQ_PUB));
          // create suberq_push
          comm->suberq_push_.reset(new zmq::socket_t(*(comm->zmq_ctx_), ZMQ_PUSH));

          try{
            comm->suberq_push_->bind(KSUBERQ_ENDP);
          }catch(zmq::error_t &e){
            comm->errcode_ = 1;
            LOG(ERROR) << "failed to connect to " << KSUBERQ_ENDP;
            pthread_mutex_lock(&(comm->sync_mtx_));
            comm->cond_pub_init_done_ = true;
	    pthread_cond_signal(&(comm->sync_cond_));
            pthread_mutex_unlock(&(comm->sync_mtx_));
            return NULL;
          }

          CommPubInitMsg *msg = (CommPubInitMsg *) data.get();

          std::string pub_ip(msg->ip_);
          std::string pub_port(msg->port_);

          std::string pub_str = std::string("tcp://") + pub_ip
                                + std::string(":") + pub_port;
          try{
            comm->pub_sock_->bind(pub_str.c_str());
          }catch(zmq::error_t &e){
            pthread_mutex_lock(&(comm->sync_mtx_));
            comm->cond_pub_init_done_ = true;
	    pthread_cond_signal(&(comm->sync_cond_));
            pthread_mutex_unlock(&(comm->sync_mtx_));
            comm->errcode_ = 1;
            LOG(ERROR) << "publish bind failed";
            return NULL;
          }
          pthread_mutex_lock(&(comm->sync_mtx_));
          comm->cond_pub_init_done_ = true;
	  pthread_cond_signal(&(comm->sync_cond_));
          pthread_mutex_unlock(&(comm->sync_mtx_));
          VLOG(2) << "CommPubInit done";
          break;
        }
        case CommSubInit:
        {
          CommSubInitMsg *msg = (CommSubInitMsg *) data.get();
          std::string sub_ip(msg->ip_);
          std::string sub_port(msg->port_);
          int32_t num_gids = msg->num_gids_;

          std::string sub_str = std::string("tcp://") + sub_ip
              + std::string(":") + sub_port;

          try{
            comm->sub_sock_->connect(sub_str.c_str());
          }catch(zmq::error_t &e){
            comm->errcode_ = 1;
            
	    pthread_mutex_lock(&(comm->sync_mtx_));
            comm->cond_subscribe_done_ = true;
	    pthread_cond_signal(&(comm->sync_cond_));
            pthread_mutex_unlock(&(comm->sync_mtx_));
            return NULL;
          }

          VLOG(2) << "to receive " << num_gids << " publish ids";
          int32_t idx_gids;

          for(idx_gids = 0; idx_gids < num_gids; ++idx_gids){
            len = RecvMsg(*comm->inter_pull_sock_, data);
            if(len < 0){
              comm->errcode_ = 1;
              pthread_mutex_lock(&(comm->sync_mtx_));
	      comm->cond_subscribe_done_ = true;
              pthread_cond_signal(&(comm->sync_cond_));
              pthread_mutex_unlock(&(comm->sync_mtx_));
              return NULL;
            }
            //VLOG(0) << "received len = " << len;
            assert(len == sizeof(int32_t));
            int32_t sub_gid = *((int32_t *) data.get());

            try{
              comm->sub_sock_->setsockopt(ZMQ_SUBSCRIBE, &sub_gid, sizeof(int32_t));
            }catch(zmq::error_t &e){
              comm->errcode_ = 1;
              pthread_mutex_lock(&(comm->sync_mtx_));
	      comm->cond_subscribe_done_ = true;
              pthread_cond_signal(&(comm->sync_cond_));
              pthread_mutex_unlock(&(comm->sync_mtx_));
              LOG(ERROR) << "set subscribe id failed";
              return NULL;
            }
          }
          // Do not signal condition variable until I actually receives a published
          // message
          break;
        }
        case CommPubInitSend:
        {
          CommPubInitSendMsg *msg = (CommPubInitSendMsg *) data.get();
          int32_t pub_init_gid = msg->gid_;

          EMsgType type = RouterPubInit;
          ret = SendMsg(*comm->pub_sock_, pub_init_gid,
                    (uint8_t *) &type, sizeof(EMsgType), ZMQ_SNDMORE);
          if(ret < 0){
            comm->errcode_ = 1;
            return NULL;
          }
          ret = SendMsg(*comm->pub_sock_, (uint8_t *) &(comm->id_),
              sizeof(int32_t), 0);
          if(ret < 0){
            comm->errcode_ = 1;
            return NULL;
          }
          break;
        }
        case CommPublish:
        {

          CommPublishMsg *msg = (CommPublishMsg *) data.get();

          EMsgType type = RouterMsg;
          ret = SendMsg(*comm->pub_sock_, msg->gid_,
                    (uint8_t *) &type, sizeof(EMsgType), ZMQ_SNDMORE);
          if(ret < 0){
            comm->errcode_ = 1;
            return NULL;
          }
          bool more;
          len = RecvMsg(*comm->inter_pull_sock_, data, &more);
          if(len < 0){
            comm->errcode_ = 1;
            return NULL;
          }

          while(more){
            ret = SendMsg(*comm->router_sock_, (uint8_t *) data.get(), len, ZMQ_SNDMORE);
            if(ret < 0){
              comm->errcode_ = 1;
              return NULL;
            }

            len = RecvMsg(*comm->inter_pull_sock_, data, &more);
            if(len < 0){
              comm->errcode_ = 1;
              return NULL;
            }
          }

          ret = SendMsg(*comm->pub_sock_, (uint8_t *) data.get(), len, 0);
          if(ret < 0){
            comm->errcode_ = 1;
            return NULL;
          }
          break;
        }
        case CommSend:
        {
          CommSendMsg *msg = (CommSendMsg *) data.get();
          CHECK_EQ(len, sizeof(CommSendMsg)) << "msg size rerror";

	  int32_t zmq_id = CidToZmqRid(msg->cid_);
	  
	  VLOG(2) << "get message to send to " << msg->cid_;

          // Before sending to a client, we have to convert client id to zmq
          // client id.
          // ZMQ requires that the last (least) byte of the client id to be
          // non-zero.

          EMsgType type = RouterMsg;

          ret = SendMsg(*comm->router_sock_, zmq_id,
              (uint8_t *) &type, sizeof(EMsgType), ZMQ_SNDMORE);
          if(ret < 0){
            comm->errcode_ = 1;
            return NULL;
          }

          bool more;
          len = RecvMsg(*comm->inter_pull_sock_, data, &more);
          if(len < 0){
            comm->errcode_ = 1;
            return NULL;
          }
          if(more) VLOG(3) << "CommSend, more = true";
          else VLOG(3) << "CommSend, more = false";
          while(more){
            ret = SendMsg(*comm->router_sock_, (uint8_t *) data.get(), len, ZMQ_SNDMORE);
            if(ret < 0){
              comm->errcode_ = 1;
              return NULL;
            }

            len = RecvMsg(*comm->inter_pull_sock_, data, &more);
            if(len < 0){
              comm->errcode_ = 1;
              return NULL;
            }
          }
          ret = SendMsg(*comm->router_sock_, (uint8_t *) data.get(), len, 0);
          if(ret < 0){
            comm->errcode_ = 1;
            return NULL;
          }
          break;
        }
        case CommThrReg:
        {
	  VLOG(1) << "CommThrReg starts";

          CommThrRegMsg *msg = (CommThrRegMsg *) data.get();
          pthread_t tid = msg->thrid_;
          zmq::socket_t *thr_push_sock;
          try{
            thr_push_sock = new zmq::socket_t(*(comm->zmq_ctx_),
                ZMQ_PUSH);
          }catch(...){
            comm->errcode_ = 1;
            LOG(ERROR) << "failed to create thr_push_sock";
	    pthread_mutex_lock(&(comm->sync_mtx_));
	    comm->cond_register_thr_done_ = true;
	    pthread_cond_signal(&(comm->sync_cond_));
	    pthread_mutex_unlock(&(comm->sync_mtx_));
            return 0;
          }

          comm->comm_thr_push_sock_[tid] = thr_push_sock;
          std::string conn_end(KTHR_RECV_ENDP_PREFIX);
          conn_end = conn_end + std::string(":") + std::to_string(tid);
          VLOG(2) << "bind to " << conn_end;
          try{
	    thr_push_sock->bind(conn_end.c_str());
          }catch(zmq::error_t &e){
	    comm->errcode_ = 1;
	    LOG(ERROR) << "thr_push_sock failed to bind";
	    pthread_mutex_lock(&(comm->sync_mtx_));
	    comm->cond_register_thr_done_ = true;
	    pthread_cond_signal(&(comm->sync_cond_));
	    pthread_mutex_unlock(&(comm->sync_mtx_));
	    return NULL;
	  }

	  pthread_mutex_lock(&(comm->sync_mtx_));
	  comm->cond_register_thr_done_ = true;
	  pthread_cond_signal(&(comm->sync_cond_));
	  pthread_mutex_unlock(&(comm->sync_mtx_));

	  VLOG(1) << "CommThrReg finished";
          break;
        }

	case CommThrDereg:
	  {
	    CommThrDeregMsg *msg = (CommThrDeregMsg *) data.get();
	    pthread_t tid = msg->thrid_;
	    delete comm->comm_thr_push_sock_[tid];
	    comm->comm_thr_push_sock_.erase(tid);

	    pthread_mutex_lock(&(comm->sync_mtx_));
	    comm->cond_deregister_thr_done_ = true;
	    pthread_cond_signal(&(comm->sync_cond_));
	    pthread_mutex_unlock(&(comm->sync_mtx_));
	    break;
	  }
	case CommThrSend:
	  {
	  CommThrSendMsg *msg = (CommThrSendMsg *) data.get();
	  int32_t zmq_id = CidToZmqRid(msg->cid_);

	  RouterThrMsgMsg router_msg;
	  router_msg.thrid_ = msg->thrid_; // destination thread id

          ret = SendMsg(*comm->router_sock_, zmq_id,
              (uint8_t *) &router_msg, sizeof(RouterThrMsgMsg), ZMQ_SNDMORE);
          if(ret < 0){
            comm->errcode_ = 1;
            return NULL;
          }

	  bool more;
          len = RecvMsg(*comm->inter_pull_sock_, data, &more);
          if(len < 0){
            comm->errcode_ = 1;
            return NULL;
          }
          while(more){
            ret = SendMsg(*comm->router_sock_, (uint8_t *) data.get(), len, 
			 ZMQ_SNDMORE);
            if(ret < 0){
              comm->errcode_ = 1;
              return NULL;
            }

            len = RecvMsg(*comm->inter_pull_sock_, data, &more);
            if(len < 0){
              comm->errcode_ = 1;
              return NULL;
            }
          }
          ret = SendMsg(*comm->router_sock_, (uint8_t *) data.get(), len, 0);
          if(ret < 0){
            comm->errcode_ = 1;
            return NULL;
          }
          
	  break;
	}
        case CommThrKeyReg:
        {
	  VLOG(2) << "CommThrKeyReg starts";

          CommThrKeyRegMsg *msg = (CommThrKeyRegMsg *) data.get();
          pthread_t tid = msg->thrid_;
	  int32_t key = msg->key_;

          zmq::socket_t *thr_key_push_sock;
          try{
            thr_key_push_sock = new zmq::socket_t(*(comm->zmq_ctx_),
                ZMQ_PUSH);
          }catch(...){
            comm->errcode_ = 1;
	    LOG(ERROR) << "failed to create thr_push_sock";
	    pthread_mutex_lock(&(comm->sync_mtx_));
	    comm->cond_register_thr_key_done_ = true;
	    pthread_cond_signal(&(comm->sync_cond_));
	    pthread_mutex_unlock(&(comm->sync_mtx_));
	
	    return 0;
          }

          (comm->comm_thr_key_push_sock_)[tid][key] = thr_key_push_sock;
          std::string conn_end(KTHR_KEY_RECV_ENDP_PREFIX);
          conn_end = conn_end + ":" + std::to_string(tid) + ":" 
	    + std::to_string(key);
          
          try{
	    thr_key_push_sock->bind(conn_end.c_str());
          }catch(zmq::error_t &e){
	    comm->errcode_ = 1;
	    LOG(ERROR) << "thr_key_push_sock failed to bind";
	    pthread_mutex_lock(&(comm->sync_mtx_));
	    comm->cond_register_thr_key_done_ = true;
	    pthread_cond_signal(&(comm->sync_cond_));
	    pthread_mutex_unlock(&(comm->sync_mtx_));
	    return NULL;
	  }
	  pthread_mutex_lock(&(comm->sync_mtx_));
	  comm->cond_register_thr_key_done_ = true;
	  pthread_cond_signal(&(comm->sync_cond_));
	  pthread_mutex_unlock(&(comm->sync_mtx_));

	  VLOG(2) << "CommThrKeyReg ends successfully";
          break;
        }
	case CommThrKeyDereg:
	  {
	    CommThrKeyDeregMsg *msg = (CommThrKeyDeregMsg *) data.get();
	    pthread_t tid = msg->thrid_;
	    int32_t key = msg->key_;
	    
	    delete (comm->comm_thr_key_push_sock_)[tid][key];
	    (comm->comm_thr_key_push_sock_)[tid].erase(key);

	    pthread_mutex_lock(&(comm->sync_mtx_));
	    comm->cond_deregister_thr_key_done_ = true;
	    pthread_cond_signal(&(comm->sync_cond_));
	    pthread_mutex_unlock(&(comm->sync_mtx_));

	    break;
	  }
	case CommThrKeySend:
	  {
	    CommThrKeySendMsg *msg = (CommThrKeySendMsg *) data.get();
	    int32_t zmq_id = CidToZmqRid(msg->cid_);
	    
	    RouterThrKeyMsgMsg router_msg;
	    router_msg.thrid_ = msg->thrid_; // destination thread id
	    router_msg.key_  = msg->key_;

	    ret = SendMsg(*comm->router_sock_, zmq_id,
			  (uint8_t *) &router_msg, sizeof(RouterThrKeyMsgMsg),
			  ZMQ_SNDMORE);
	    if(ret < 0){
	      comm->errcode_ = 1;
	      return NULL;
	    }

	    bool more;
	    len = RecvMsg(*comm->inter_pull_sock_, data, &more);
	    if(len < 0){
	      comm->errcode_ = 1;
	      return NULL;
	    }
	    while(more){
	      ret = SendMsg(*comm->router_sock_, (uint8_t *) data.get(), len, 
			    ZMQ_SNDMORE);
	      if(ret < 0){
              comm->errcode_ = 1;
              return NULL;
	      }
	      
	      len = RecvMsg(*comm->inter_pull_sock_, data, &more);
	      if(len < 0){
              comm->errcode_ = 1;
              return NULL;
	      }
	    }
	    ret = SendMsg(*comm->router_sock_, (uint8_t *) data.get(), len, 0);
	    if(ret < 0){
	      comm->errcode_ = 1;
	      return NULL;
	    }
	    
	    break;
	  }
        case CommShutDown:
	 VLOG(1) << "CommShutDown";
          // shut down comm thread
          return NULL;
          break;
        default:
          assert(0);
          break;
        }
        continue;
      }

      /* router_sock_ events START */
      if(pollitems[1].revents){
        VLOG(3) << "router_sock_ gets a message!" << std::endl;
        boost::shared_array<uint8_t> data;
        int len;
        EMsgType router_type;
        int32_t zmq_cid;

        len = RecvMsg(*comm->router_sock_, zmq_cid, data);
        if(len < 0){
          comm->errcode_ = 1;
          return NULL;
        }

        router_type = *((EMsgType *) data.get());

        int32_t cid = ZmqRidToCid(zmq_cid);

        switch(router_type){
        case RouterConnect:
          // A client has connected to myself.
          // connect || reconnect

          VLOG(2) << "received RouterConnect! from " << cid << std::endl;
          if(comm->clientmap_.count(cid) == 0 || comm->clientmap_[cid] == false){
            comm->clientmap_[cid] = true;
            comm->clientq_.Push(cid);
            VLOG(2) << "received connection from client " << cid << std::endl;
          }
          break;

        case RouterPubInitReply:
          ret = SendMsg(*comm->suberq_push_, (uint8_t *) &cid, sizeof(int32_t), 
			0);
          if(ret < 0){
            comm->errcode_ = 1;
            return NULL;
          }
          VLOG(2) << "send to suberq_push successfully";
          break;
        case RouterMsg:
        {
          bool more;
          VLOG(3) << "received RouterMsg! from " << cid;
          len = RecvMsg(*comm->router_sock_, data, &more);
          if(len <= 0){
            comm->errcode_ = 1;
	    LOG(FATAL) << "receive data len = " << len;
            return NULL;
          }
	  
	  // first send out client id
          ret = SendMsg(*comm->inter_push_sock_, (uint8_t *) &cid, 
			sizeof(int32_t), ZMQ_SNDMORE);
          if(ret != sizeof(int32_t)){
            comm->errcode_ = 1;
	    LOG(FATAL) << "send client id ret = " << ret;
            return NULL;
          }

          VLOG(3) << "Send RouterMsg to inter_push_sock_";

          while(more){
	    
            ret = SendMsg(*comm->inter_push_sock_, (uint8_t *) data.get(), len, 
			  ZMQ_SNDMORE);
            if(ret != len){
              comm->errcode_ = 1;
	      LOG(FATAL) << "send message size = " << ret
			 << " should be " << len;
              return NULL;
            }

            len = RecvMsg(*comm->router_sock_, data, &more);
            if(len <= 0){
              comm->errcode_ = 1;
              return NULL;
            }
          }

          ret = SendMsg(*comm->inter_push_sock_, (uint8_t *) data.get(), len, 0);
          
	  if(ret < 0){
	    comm->errcode_ = 1;
	    LOG(FATAL) << "send message size = " << ret 
		       << " should be " << len;
	    return NULL;
	  }

	  VLOG(3) << "Send RouterMsg lasts part to inter_push_sock_, ret = "
		    << ret;

          break;
        }
	case RouterThrMsg:
	  {
	    VLOG(3) << "RouterThrMsg starts";
	    RouterThrMsgMsg *msg = (RouterThrMsgMsg *) data.get();
	    pthread_t thrid = msg->thrid_;
	    zmq::socket_t *push_sock = comm->comm_thr_push_sock_[thrid];
	    
	    if(push_sock == NULL){
	      LOG(WARNING) << " received thread-specific targetted non-exist" 
			   << " thread id = " << thrid;
	      // clean up the rest of the message from the socket
	      bool more;
	      len = RecvMsg(*comm->router_sock_, data, &more);
	      if(len < 0){
		comm->errcode_ = 1;
		return NULL;
	      }
	      while(more){
		len = RecvMsg(*comm->router_sock_, data, &more);
		if(len < 0){
		  comm->errcode_ = 1;
		  return NULL;
		}
	      }
	    }
	    
	    bool more;
	    len = RecvMsg(*comm->router_sock_, data, &more);
	    if(len < 0){
	      comm->errcode_ = 1;
	      return NULL;
	    }

	    ret = SendMsg(*push_sock, (uint8_t *) &cid, 
			  sizeof(int32_t), ZMQ_SNDMORE);
	    if(ret < 0){
	      comm->errcode_ = 1;
	      return NULL;
	    }

	    while(more){
	      ret = SendMsg(*push_sock, (uint8_t *) data.get(), len, ZMQ_SNDMORE);
	      if(ret < 0){
		comm->errcode_ = 1;
		return NULL;
	      }

	      len = RecvMsg(*comm->router_sock_, data, &more);
	      if(len < 0){
		comm->errcode_ = 1;
		return NULL;
	      }
	    }
	    ret = SendMsg(*push_sock, (uint8_t *) data.get(), len, 0);
	    if(ret < 0){
	      comm->errcode_ = 1;
	      return NULL;
	    }
	    break; 
	  }
	case RouterThrKeyMsg:
	  {
	    RouterThrKeyMsgMsg *msg = (RouterThrKeyMsgMsg *) data.get();
	    pthread_t thrid = msg->thrid_;
	    int32_t key = msg->key_;

	    zmq::socket_t *push_sock = comm->comm_thr_key_push_sock_[thrid][key];
	    
	    if(push_sock == NULL){
	      LOG(WARNING) << " received thread-specific keyed message"
                           <<  " targetted non-exist thread ";
	      // clean up the rest of the message from the socket
	      bool more;
	      len = RecvMsg(*comm->router_sock_, data, &more);
	      if(len < 0){
		comm->errcode_ = 1;
		return NULL;
	      }
	      while(more){
		len = RecvMsg(*comm->router_sock_, data, &more);
		if(len < 0){
		  comm->errcode_ = 1;
		  return NULL;
		}
	      }
	    }

	    ret = SendMsg(*push_sock, (uint8_t *) &cid, 
			  sizeof(int32_t), ZMQ_SNDMORE);
	    if(ret < 0){
	      comm->errcode_ = 1;
	      return NULL;
	    }

	    bool more;
	    len = RecvMsg(*comm->router_sock_, data, &more);
	    if(len < 0){
	      comm->errcode_ = 1;
	      return NULL;
	    }
	    
	    while(more){
	      ret = SendMsg(*push_sock, (uint8_t *) data.get(), len, ZMQ_SNDMORE);
	      if(ret < 0){
		comm->errcode_ = 1;
		return NULL;
	      }

	      len = RecvMsg(*comm->router_sock_, data, &more);
	      if(len < 0){
		comm->errcode_ = 1;
		return NULL;
	      }
	    }
	    VLOG(3) << "received thread-specific keyed-message, sent " << len 
		      << " bytes";
	    ret = SendMsg(*push_sock, (uint8_t *) data.get(), len, 0);
	    if(ret < 0){
	      comm->errcode_ = 1;
	      return NULL;
	    }
	    break; 
	  }
        default:
          assert(0);
          break;
        }
        continue;
      }

      if(pollitems[2].revents){
        VLOG(3) << "monitor_sock_ gets a message!" << std::endl;

        zmq_event_t *event;
        boost::shared_array<uint8_t> data;
        int len;
        len = RecvMsg(*comm->monitor_sock_, data);

        if (len < 0){
          comm->errcode_ = 1;
          return NULL;
        }
        assert(len == sizeof(zmq_event_t));
        event = (zmq_event_t *) data.get();

        switch (event->event){
        case ZMQ_EVENT_CONNECTED:
          VLOG(3) << "router_sock_ connected to remote client";
          break;
        case ZMQ_EVENT_ACCEPTED:
          VLOG(3) << "monitor_socsk_: accepted connection.";
          break;
        case ZMQ_EVENT_DISCONNECTED:
          LOG(INFO) << "monitor_socsk_: client disconnected.";
          break;
        default:
          LOG(ERROR) << "unexpected event: " << event->event;
          comm->errcode_ = 1;
          return NULL;
        }
        continue;
      }

      if(pollitems[3].events){
        VLOG(3) << "sub_sock_ gets a message!" << std::endl;
        boost::shared_array<uint8_t> data;
        int len, ret;
        EMsgType router_type, type;
        int32_t zmq_cid, cid;
        len = RecvMsg(*comm->sub_sock_, zmq_cid, data);
        if(len < 0){
          comm->errcode_ = 1;
          return NULL;
        }
        cid = ZmqRidToCid(zmq_cid);
        router_type = *((EMsgType *) data.get());
        switch(router_type){
        case RouterPubInit:
        {
          // I might see the RouterPubInit message many times as the publisher
          // sends it multiple times. However, I need to make sure I send only
          // one response back as the publisher relies on the count of responses
          //  to know if there are enough subscribers.
          // Keep publishers in a map so I know if I have received from it.
          VLOG(3) << "type = RouterPubInit";
          len = RecvMsg(*comm->sub_sock_, data);
          if(len < 0){
            comm->errcode_ = 1;
            return NULL;
          }
          int32_t publisher_id = *((int32_t *) data.get());
          if(comm->publishermap_.count(publisher_id) == 0){
            VLOG(3) << "send to publisher_id = " << publisher_id;
            type = RouterPubInitReply;
            int32_t pub_zmq_id = CidToZmqRid(publisher_id);
            ret = SendMsg(*comm->router_sock_, pub_zmq_id, (uint8_t *) &type,
                sizeof(EMsgType), 0);
            if(ret < 0){
              comm->errcode_ = 1;
              return NULL;
            }
            comm->publishermap_[cid] = true;
            // client thread is still waiting at SubscribeTo()
            pthread_mutex_lock(&(comm->sync_mtx_));
            comm->cond_subscribe_done_ = true;
	    pthread_cond_signal(&(comm->sync_cond_));
            pthread_mutex_unlock(&(comm->sync_mtx_));
          }
          break;
        }
        case RouterMsg:
        {
          bool more;
          len = RecvMsg(*comm->sub_sock_, data, &more);
          if(len < 0){
            comm->errcode_ = 1;
            return NULL;
          }

          ret = SendMsg(*comm->inter_push_sock_, (uint8_t *) &cid, sizeof(int32_t), ZMQ_SNDMORE);
          if(ret < 0){
            comm->errcode_ = 1;
            return NULL;
          }
          while(more){
            ret = SendMsg(*comm->inter_push_sock_, (uint8_t *) data.get(), len, ZMQ_SNDMORE);
            if(ret < 0){
              comm->errcode_ = 1;
              return NULL;
            }

            len = RecvMsg(*comm->sub_sock_, data, &more);
            if(len < 0){
              comm->errcode_ = 1;
              return NULL;
            }
          }
          SendMsg(*comm->inter_push_sock_, (uint8_t *) data.get(), len, 0);
          break;
        }
        default:
          assert(0);
          break;
        }
        continue;
      }
    }
    return NULL;
  }
}
