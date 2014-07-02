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
#pragma once

#include <zmq.hpp>
#include <assert.h>
#include <stdint.h>
#include <time.h>

namespace petuum {

class ZMQUtil {
public:
  static int32_t EntityID2ZmqID(int32_t entity_id);
  
  static int32_t ZmqID2EntityID(int32_t zmq_id);
  
  static void ZMQSetSockOpt (zmq::socket_t *sock, int option, const void *optval,
    size_t optval_size);
  
  static void ZMQBind(zmq::socket_t *sock, const std::string &connect_addr);

  static void ZMQConnectSend(zmq::socket_t *sock, const std::string &connect_addr, 
    int32_t zmq_id, void *msg, size_t size);

  // True for received, false for not
  static bool ZMQRecvAsync(zmq::socket_t *sock, zmq::message_t *msg);

  static bool ZMQRecvAsync(zmq::socket_t *sock, int32_t *zmq_id, zmq::message_t *msg);

  static void ZMQRecv(zmq::socket_t *sock, zmq::message_t *msg);
  
  static void ZMQRecv(zmq::socket_t *sock, int32_t *zmq_id, zmq::message_t *msg);

  /*
   * return number of bytes sent
   */
  static size_t ZMQSend(zmq::socket_t *sock, const void *data, size_t len, int flag = 0);

  // 0 means cannot be sent, try again; 
  // should not happen unless flag = ZMQ_DONTWAIT
  static size_t ZMQSend(zmq::socket_t *sock, int32_t zmq_id, const void *data, 
    size_t len, int flag = 0);

  // msg is nollified during the call
  static size_t ZMQSend(zmq::socket_t *sock, zmq::message_t &msg, int flag = 0);

  static size_t ZMQSend(zmq::socket_t *sock, int32_t zmq_id, 
    zmq::message_t &msg, int flag = 0);


};
}
