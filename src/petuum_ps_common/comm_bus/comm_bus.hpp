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
// Author: Jinliang Wei

#pragma once

#include <petuum_ps_common/comm_bus/zmq_util.hpp>
#include <zmq.hpp>
#include <string>
#include <utility>
#include <boost/thread/tss.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/scoped_array.hpp>
#include <boost/noncopyable.hpp>

namespace petuum {

/*
 * This class resembles a shared bus among all local theads and remote
 * threads.  The goal is to simplify communication handling and unify
 * in-process and network communication under the same interface. Each thread
 * is treated as an unique entity and has its unique global (among all hosts)
 * ID. The ID is the concatnation of the thread's host ID and the thread's
 * local ID.  As it is meant to be simple, it is not doing much error checking
 * and recovery. If something goes wrong, it fails (aborts) quickly to allow
 * debugging to happen immediately.
 * Each thread is an entity and should only register (ThreadRegister) once.
 * A thread is local if it is in the same CommBus object as myself, otherwise it
 * is remote.
 */

class CommBus : boost::noncopyable {
public:
  static const int kNone = 0;
  static const int kInProc = 1;
  static const int kInterProc = 2;

  struct Config : boost::noncopyable {
  public:
    // My thread id.
    int32_t entity_id_;

    // What should I listen to?
    int ltype_;

    // In the format of "ip:port", such as "192.168.1.1:9999". It must be set
    // if ((ltype_ & kInterProc) == true)
    std::string network_addr_;

    int num_bytes_inproc_send_buff_;
    int num_bytes_inproc_recv_buff_;
    int num_bytes_interproc_send_buff_;
    int num_bytes_interproc_recv_buff_;

    Config():
      entity_id_(0),
      ltype_(kNone),
      num_bytes_inproc_send_buff_(0),
      num_bytes_inproc_recv_buff_(0),
      num_bytes_interproc_send_buff_(0),
      num_bytes_interproc_recv_buff_(0) { }

    Config(int32_t entity_id, int ltype, std::string network_addr):
      entity_id_(entity_id),
      ltype_(ltype),
      network_addr_(network_addr),
      num_bytes_inproc_send_buff_(0),
      num_bytes_inproc_recv_buff_(0),
      num_bytes_interproc_send_buff_(0),
      num_bytes_interproc_recv_buff_(0) { }
  };

  struct ThreadCommInfo : boost::noncopyable {
  public:
    int32_t entity_id_;
    boost::scoped_ptr<zmq::socket_t> inproc_sock_;
    boost::scoped_ptr<zmq::socket_t> interproc_sock_;
    boost::scoped_array<zmq::pollitem_t> pollitems_;  // Only contains those
                                                      // listening sockets
    boost::scoped_ptr<zmq::pollitem_t> inproc_pollitem_;
    boost::scoped_ptr<zmq::pollitem_t> interproc_pollitem_;
    int ltype_;
    int32_t poll_size_;

    int num_bytes_inproc_send_buff_;
    int num_bytes_inproc_recv_buff_;
    int num_bytes_interproc_send_buff_;
    int num_bytes_interproc_recv_buff_;

    ThreadCommInfo() { }
  };

  bool IsLocalEntity(int32_t entity_id);

  CommBus(int32_t e_st, int32_t e_end, int32_t num_zmq_thrs = 1);
  ~CommBus();

  // Register a thread, set up necessary commnication channel.
  // For network communication (TCP), zmq::bind may be called after
  // zmq::connect. But for inproc communication, zmq::bind has to be called
  // before zmq::connect.
  // For CommBus, a thread must have successfully registered before
  // other thread may connect to it.
  void ThreadRegister(const Config &config);
  void ThreadDeregister();

  // Connect to a local thread
  // Info is a customer-defined number to be included in the Connect message,
  // how to use it is up to the customer.
  void ConnectTo(int32_t entity_id, void *connect_msg, size_t size);
  // Connect to a remote thread.
  void ConnectTo(int32_t entity_id, const std::string& network_addr,
    void *connect_msg, size_t size);

  size_t Send(int32_t entity_id, const void *data, size_t len);
  size_t SendInProc(int32_t entity_id, const void *data, size_t len);
  size_t SendInterProc(int32_t entity_id, const void *data, size_t len);

  // msg is nollified
  size_t Send(int32_t entity_id, zmq::message_t &msg);
  size_t SendInProc(int32_t entity_id, zmq::message_t &msg);

  void Recv(int32_t *entity_id, zmq::message_t *msg);
  bool RecvAsync(int32_t *entity_id, zmq::message_t *msg);
  bool RecvTimeOut(int32_t *entity_id, zmq::message_t *msg, long timeout_milli);

  void RecvInProc(int32_t *entity_id, zmq::message_t *msg);
  bool RecvInProcAsync(int32_t *entity_id, zmq::message_t *msg);
  bool RecvInProcTimeOut(int32_t *entity_id, zmq::message_t *msg,
      long timeout_milli);

  void RecvInterProc(int32_t *entity_id, zmq::message_t *msg);
  bool RecvInterProcAsync(int32_t *entity_id, zmq::message_t *msg);
  bool RecvInterProcTimeOut(int32_t *entity_id, zmq::message_t *msg,
      long timeout_milli);
  typedef void (CommBus::*RecvFunc)(int32_t *sender_id,
    zmq::message_t *zmq_msg);
  typedef bool (CommBus::*RecvTimeOutFunc)(int32_t *sender_id,
    zmq::message_t *zmq_msg, long timeout_milli);
  typedef bool (CommBus::*RecvAsyncFunc)(int32_t *sender_id,
                                         zmq::message_t *msg);

  typedef void (*RecvWrapperFunc)(int32_t *sender_id, zmq::message_t *msg);

  typedef size_t (CommBus::*SendFunc)(int32_t entity_id, const void *msg,
    size_t len);

private:
  static void MakeInProcAddr(int32_t entity_id, std::string *result);
  static void MakeInterProcAddr(const std::string &network_addr,
      std::string *result);

  static void SetUpRouterSocket(zmq::socket_t *sock, int32_t id,
    int num_bytes_send_buff, int num_bytes_recv_buff);
  static const std::string kInProcPrefix;
  static const std::string kInterProcPrefix;
  zmq::context_t *zmq_ctx_;
  // denote the range of entity IDs that are local, inclusive
  int32_t e_st_;
  int32_t e_end_;
  boost::thread_specific_ptr<ThreadCommInfo> thr_info_;
};
}   // namespace petuum
