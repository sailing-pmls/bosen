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

#ifndef __PETUUM_ZMQ_UTIL_HPP__
#define __PETUUM_ZMQ_UTIL_HPP__

#include <zmq.hpp>
#include <assert.h>
#include <boost/shared_array.hpp>
#include <stdint.h>
namespace petuum {

  int32_t CidToZmqRid(int32_t _cid);

  int32_t ZmqRidToCid(int32_t _zid);

  /*
   * return number of bytes received, negative if error
   */
  int RecvMsgAsync(zmq::socket_t &_sock, boost::shared_array<uint8_t> &_data,
      bool *_more = NULL);

  /*
   * return number of bytes received, negative if error
   */
  int RecvMsgAsync(zmq::socket_t &_sock, int32_t &_cid,
      boost::shared_array<uint8_t> &_data, bool *_more = NULL);

  /*
   * return number of bytes received, negative if error
   */
  int RecvMsg(zmq::socket_t &_sock, boost::shared_array<uint8_t> &_data,
      bool *_more = NULL);

  /*
   * return number of bytes received, negative if error
   */
  int RecvMsg(zmq::socket_t &_sock, int32_t &_cid,
      boost::shared_array<uint8_t> &_data, bool *_more = NULL);


  /*
   * return number of bytes sent, negative if error
   */
  int SendMsg(zmq::socket_t &_sock, const uint8_t *_data, size_t _len, int _flag);

  int SendMsg(zmq::socket_t &_sock, int32_t _cid, const uint8_t *_data, size_t _len, int _flag);
}
#endif
