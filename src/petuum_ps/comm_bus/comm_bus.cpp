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
// author: jinliang

#include "petuum_ps/comm_bus/comm_bus.hpp"
#include "petuum_ps/comm_bus/zmq_util.hpp"
#include "petuum_ps/thread/ps_msgs.hpp"
#include <stdlib.h>
#include <glog/logging.h>
#include <sstream>
#include <string>

namespace petuum {

const std::string CommBus::kInProcPrefix("inproc://comm_bus");
const std::string CommBus::kInterProcPrefix("tcp://");

void CommBus::MakeInProcAddr(int32_t entity_id, std::string *result) {
  std::stringstream ss;
  ss << entity_id;
  *result = kInProcPrefix;
  *result += ":";
  *result += ss.str();
}

void CommBus::MakeInterProcAddr(const std::string &network_addr,
  std::string *result) {
  *result = kInterProcPrefix;
  *result += network_addr;
}

bool CommBus::IsLocalEntity(int32_t entity_id) {
  //VLOG(0) << "e_st_ = " << e_st_
  //	  << " e_end_ = " << e_end_;
  return (e_st_ <= entity_id) && (entity_id <= e_end_);
}


CommBus::CommBus(int32_t e_st, int32_t e_end, int32_t num_zmq_thrs) {
  e_st_ = e_st;
  e_end_ = e_end;

  try {
    zmq_ctx_ = new zmq::context_t(num_zmq_thrs);
  } catch(zmq::error_t &e) {
    LOG(FATAL) << "Faield to create zmq context " << e.what();
  } catch(...) {
    LOG(FATAL) << "Failed to create zmq context";
  }
}

CommBus::~CommBus() {
  delete zmq_ctx_;
}

void CommBus::SetUpRouterSocket(zmq::socket_t *sock, int32_t id,
  int num_bytes_send_buff, int num_bytes_recv_buff) {
  int32_t my_id = ZMQUtil::EntityID2ZmqID(id);
  ZMQUtil::ZMQSetSockOpt(sock, ZMQ_IDENTITY, &my_id, sizeof(my_id));

  int sock_mandatory = 1;
  ZMQUtil::ZMQSetSockOpt(sock, ZMQ_ROUTER_MANDATORY, &(sock_mandatory),
    sizeof(sock_mandatory));

  if (num_bytes_send_buff != 0) {
    ZMQUtil::ZMQSetSockOpt(sock, ZMQ_SNDBUF, &(num_bytes_send_buff),
      sizeof(num_bytes_send_buff));
  }

  if (num_bytes_recv_buff != 0) {
    ZMQUtil::ZMQSetSockOpt(sock, ZMQ_RCVBUF, &(num_bytes_recv_buff),
        sizeof(num_bytes_recv_buff));
  }
}

void CommBus::ThreadRegister(const Config &config) {
  CHECK(NULL == thr_info_.get()) << "This thread has been initialized";

  thr_info_.reset(new ThreadCommInfo());
  thr_info_->entity_id_ = config.entity_id_;
  thr_info_->ltype_ = config.ltype_;

  thr_info_->num_bytes_inproc_send_buff_ = config.num_bytes_inproc_send_buff_;
  thr_info_->num_bytes_inproc_recv_buff_ = config.num_bytes_inproc_recv_buff_;

  thr_info_->num_bytes_interproc_send_buff_ =
    config.num_bytes_interproc_send_buff_;
  thr_info_->num_bytes_interproc_recv_buff_ =
    config.num_bytes_interproc_recv_buff_;

  VLOG(0) << "CommBus ThreadRegister()";

  if (config.ltype_ & kInProc) {
    try {
      thr_info_->inproc_sock_.reset(new zmq::socket_t(*zmq_ctx_, ZMQ_ROUTER));
    } catch(...) {
      LOG(FATAL) << "Failed creating router socket";
    }

    zmq::socket_t *sock = thr_info_->inproc_sock_.get();

    SetUpRouterSocket(sock, config.entity_id_,
        config.num_bytes_inproc_send_buff_,
        config.num_bytes_inproc_recv_buff_);

    std::string bind_addr;
    MakeInProcAddr(config.entity_id_, &bind_addr);

    ZMQUtil::ZMQBind(sock, bind_addr);
  }

  if (config.ltype_ & kInterProc) {
    try {
      thr_info_->interproc_sock_.reset(
          new zmq::socket_t(*zmq_ctx_, ZMQ_ROUTER));
    } catch(...) {
      LOG(FATAL) << "Failed creating router socket";
    }

    zmq::socket_t *sock = thr_info_->interproc_sock_.get();

    SetUpRouterSocket(sock, config.entity_id_,
        config.num_bytes_inproc_send_buff_,
        config.num_bytes_inproc_recv_buff_);

    std::string bind_addr;
    MakeInterProcAddr(config.network_addr_, &bind_addr);

    ZMQUtil::ZMQBind(sock, bind_addr);
  }
}

void CommBus::ThreadDeregister() {
  thr_info_.reset();
}

void CommBus::ConnectTo(int32_t entity_id, void *connect_msg, size_t size) {
  CHECK(IsLocalEntity(entity_id)) << "Not local entity " << entity_id;

  zmq::socket_t *sock = thr_info_->inproc_sock_.get();
  if (sock == NULL) {
    try {
      thr_info_->inproc_sock_.reset(new zmq::socket_t(*zmq_ctx_, ZMQ_ROUTER));
    } catch (...) {
      LOG(FATAL) << "Failed creating router socket";
    }
    sock = thr_info_->inproc_sock_.get();

    SetUpRouterSocket(sock, thr_info_->entity_id_,
        thr_info_->num_bytes_inproc_send_buff_,
        thr_info_->num_bytes_inproc_recv_buff_);
  }
  std::string connect_addr;
  MakeInProcAddr(entity_id, &connect_addr);
  int32_t zmq_id = ZMQUtil::EntityID2ZmqID(entity_id);
  ZMQUtil::ZMQConnectSend(sock, connect_addr, zmq_id, connect_msg, size);
}

void CommBus::ConnectTo(int32_t entity_id, const std::string &network_addr,
    void *connect_msg, size_t size) {
  CHECK(!IsLocalEntity(entity_id)) << "Local entity " << entity_id;

  zmq::socket_t *sock = thr_info_->interproc_sock_.get();
  if (sock == NULL) {
    try {
      thr_info_->interproc_sock_.reset(new zmq::socket_t(*zmq_ctx_,
        ZMQ_ROUTER));
    } catch (...) {
      LOG(FATAL) << "Failed creating router socket";
    }
    sock = thr_info_->interproc_sock_.get();

    SetUpRouterSocket(sock, thr_info_->entity_id_,
        thr_info_->num_bytes_interproc_send_buff_,
        thr_info_->num_bytes_interproc_recv_buff_);
  }

  std::string connect_addr;
  MakeInterProcAddr(network_addr, &connect_addr);
  int32_t zmq_id = ZMQUtil::EntityID2ZmqID(entity_id);
  ZMQUtil::ZMQConnectSend(sock, connect_addr, zmq_id, connect_msg, size);
}

size_t CommBus::Send(int32_t entity_id, const void *data, size_t len) {
  zmq::socket_t *sock;

  if (IsLocalEntity(entity_id)) {
    sock = thr_info_->inproc_sock_.get();
  } else {
    sock = thr_info_->interproc_sock_.get();
  }

  int32_t recv_id = ZMQUtil::EntityID2ZmqID(entity_id);
  size_t nbytes = ZMQUtil::ZMQSend(sock, recv_id, data, len, 0);

  return nbytes;
}

size_t CommBus::SendInProc(int32_t entity_id, const void *data, size_t len) {
  zmq::socket_t *sock = thr_info_->inproc_sock_.get();

  int32_t recv_id = ZMQUtil::EntityID2ZmqID(entity_id);
  size_t nbytes = ZMQUtil::ZMQSend(sock, recv_id, data, len, 0);

  return nbytes;
}

size_t CommBus::SendInterProc(int32_t entity_id, const void *data, size_t len) {
  zmq::socket_t *sock = thr_info_->interproc_sock_.get();

  int32_t recv_id = ZMQUtil::EntityID2ZmqID(entity_id);
  size_t nbytes = ZMQUtil::ZMQSend(sock, recv_id, data, len, 0);

  return nbytes;
}

size_t CommBus::Send(int32_t entity_id, zmq::message_t &msg) {
  zmq::socket_t *sock;

  if (IsLocalEntity(entity_id)) {
    sock = thr_info_->inproc_sock_.get();
  } else {
    sock = thr_info_->interproc_sock_.get();
  }

  int32_t recv_id = ZMQUtil::EntityID2ZmqID(entity_id);
  size_t nbytes = ZMQUtil::ZMQSend(sock, recv_id, msg, 0);

  return nbytes;
}

size_t CommBus::SendInProc(int32_t entity_id, zmq::message_t &msg) {
  zmq::socket_t *sock = thr_info_->inproc_sock_.get();

  int32_t recv_id = ZMQUtil::EntityID2ZmqID(entity_id);
  size_t nbytes = ZMQUtil::ZMQSend(sock, recv_id, msg, 0);

  return nbytes;
}


void CommBus::Recv(int32_t *entity_id, zmq::message_t *msg) {
  if (thr_info_->pollitems_.get() == NULL) {
    thr_info_->pollitems_.reset(new zmq::pollitem_t[2]);
    thr_info_->pollitems_[0].socket = *(thr_info_->inproc_sock_);
    thr_info_->pollitems_[0].events = ZMQ_POLLIN;
    thr_info_->pollitems_[1].socket = *(thr_info_->interproc_sock_);
    thr_info_->pollitems_[1].events = ZMQ_POLLIN;
  }

  zmq::poll(thr_info_->pollitems_.get(), 2);
  zmq::socket_t *sock;
  if (thr_info_->pollitems_[0].revents) {
    sock = thr_info_->inproc_sock_.get();
  } else {
    sock = thr_info_->interproc_sock_.get();
  }

  int32_t sender_id;
  ZMQUtil::ZMQRecv(sock, &sender_id, msg);
  *entity_id = ZMQUtil::ZmqID2EntityID(sender_id);
}

bool CommBus::RecvAsync(int32_t *entity_id, zmq::message_t *msg) {
  if (thr_info_->pollitems_.get() == NULL) {
    thr_info_->pollitems_.reset(new zmq::pollitem_t[2]);
    thr_info_->pollitems_[0].socket = *(thr_info_->inproc_sock_);
    thr_info_->pollitems_[0].events = ZMQ_POLLIN;
    thr_info_->pollitems_[1].socket = *(thr_info_->interproc_sock_);
    thr_info_->pollitems_[1].events = ZMQ_POLLIN;
  }

  zmq::poll(thr_info_->pollitems_.get(), 2, 0);
  zmq::socket_t *sock;
  if (thr_info_->pollitems_[0].revents) {
    sock = thr_info_->inproc_sock_.get();
  } else if (thr_info_->pollitems_[1].revents) {
    sock = thr_info_->interproc_sock_.get();
  } else {
    return false;
  }

  int32_t sender_id;
  ZMQUtil::ZMQRecv(sock, &sender_id, msg);
  *entity_id = ZMQUtil::ZmqID2EntityID(sender_id);
  return true;
}

bool CommBus::RecvTimeOut(int32_t *entity_id, zmq::message_t *msg,
    long timeout_milli) {
  if (thr_info_->pollitems_.get() == NULL) {
    thr_info_->pollitems_.reset(new zmq::pollitem_t[2]);
    thr_info_->pollitems_[0].socket = *(thr_info_->inproc_sock_);
    thr_info_->pollitems_[0].events = ZMQ_POLLIN;
    thr_info_->pollitems_[1].socket = *(thr_info_->interproc_sock_);
    thr_info_->pollitems_[1].events = ZMQ_POLLIN;
  }

  zmq::poll(thr_info_->pollitems_.get(), 2, timeout_milli);
  zmq::socket_t *sock;
  if (thr_info_->pollitems_[0].revents) {
    sock = thr_info_->inproc_sock_.get();
  } else if (thr_info_->pollitems_[1].revents) {
    sock = thr_info_->interproc_sock_.get();
  } else {
    return false;
  }

  int32_t sender_id;
  ZMQUtil::ZMQRecv(sock, &sender_id, msg);
  *entity_id = ZMQUtil::ZmqID2EntityID(sender_id);
  return true;
}

void CommBus::RecvInProc(int32_t *entity_id, zmq::message_t *msg) {
  int32_t sender_id;
  ZMQUtil::ZMQRecv(thr_info_->inproc_sock_.get(), &sender_id, msg);
  *entity_id = ZMQUtil::ZmqID2EntityID(sender_id);
}

bool CommBus::RecvInProcAsync(int32_t *entity_id, zmq::message_t *msg) {
  int32_t sender_id;
  bool recved = ZMQUtil::ZMQRecvAsync(thr_info_->inproc_sock_.get(),
      &sender_id, msg);

  if (recved) {
    *entity_id = ZMQUtil::ZmqID2EntityID(sender_id);
  }
  return recved;
}

bool CommBus::RecvInProcTimeOut(int32_t *entity_id, zmq::message_t *msg,
    long timeout_milli) {
  if (thr_info_->inproc_pollitem_.get() == NULL) {
    thr_info_->inproc_pollitem_.reset(new zmq::pollitem_t);
    thr_info_->inproc_pollitem_->socket = *(thr_info_->inproc_sock_);
    thr_info_->inproc_pollitem_->events = ZMQ_POLLIN;
  }

  zmq::poll(thr_info_->inproc_pollitem_.get(), 1, timeout_milli);
  zmq::socket_t *sock;
  if (thr_info_->inproc_pollitem_->revents) {
    sock = thr_info_->inproc_sock_.get();
  } else {
    return false;
  }

  int32_t sender_id;
  ZMQUtil::ZMQRecv(sock, &sender_id, msg);
  *entity_id = ZMQUtil::ZmqID2EntityID(sender_id);
  return true;
}

void CommBus::RecvInterProc(int32_t *entity_id, zmq::message_t *msg) {
  int32_t sender_id;
  ZMQUtil::ZMQRecv(thr_info_->interproc_sock_.get(), &sender_id, msg);
  *entity_id = ZMQUtil::ZmqID2EntityID(sender_id);
}

bool CommBus::RecvInterProcAsync(int32_t *entity_id, zmq::message_t *msg) {
  int32_t sender_id;
  bool recved = ZMQUtil::ZMQRecvAsync(thr_info_->interproc_sock_.get(),
      &sender_id, msg);

  if (recved) {
    *entity_id = ZMQUtil::ZmqID2EntityID(sender_id);
  }
  return recved;
}

bool CommBus::RecvInterProcTimeOut(int32_t *entity_id, zmq::message_t *msg,
    long timeout_milli) {
  if (thr_info_->interproc_pollitem_.get() == NULL) {
    thr_info_->interproc_pollitem_.reset(new zmq::pollitem_t);
    thr_info_->interproc_pollitem_->socket = *(thr_info_->interproc_sock_);
    thr_info_->interproc_pollitem_->events = ZMQ_POLLIN;
  }

  zmq::poll(thr_info_->interproc_pollitem_.get(), 1, timeout_milli);
  zmq::socket_t *sock;
  if (thr_info_->interproc_pollitem_->revents) {
    sock = thr_info_->interproc_sock_.get();
  } else {
    return false;
  }

  int32_t sender_id;
  ZMQUtil::ZMQRecv(sock, &sender_id, msg);
  *entity_id = ZMQUtil::ZmqID2EntityID(sender_id);
  return true;
}

}   // namespace petuum
