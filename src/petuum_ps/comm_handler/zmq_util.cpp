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


#include "zmq_util.hpp"
#include <glog/logging.h>

namespace petuum {

  int32_t CidToZmqRid(int32_t cid){
    return (cid << 4 | 0x1);
  }

  int32_t ZmqRidToCid(int32_t zid){
    return (zid >> 4);
  }

  /*
   * return number of bytes received, negative if error, 0 for received nothing,
   *  which should be treated as error
   */
  int32_t RecvMsg(zmq::socket_t &sock, boost::shared_array<uint8_t> &data,
      bool *_more){
    zmq::message_t msgt;
    int nbytes;
    try{
      nbytes = sock.recv(&msgt);
    }catch(zmq::error_t &e){
      LOG(ERROR) << "RecvMsg error = " << e.what();
      return -1;
    }

    if(nbytes == 0) return 0;

    size_t len = msgt.size();
    CHECK(len > 0) << "message size should be positive, len = " << len
		   << " nbytes = " << nbytes;

    uint8_t *dataptr;
    try{
      dataptr = new uint8_t[len];
    }catch(std::bad_alloc &e){
      LOG(ERROR) << "Failed allocating memory!";
      return -1;
    }
    
    memcpy(dataptr, msgt.data(), len);
    data.reset(dataptr);

    if(_more != NULL){
      int more_recv;
      size_t s = sizeof(int);
      sock.getsockopt(ZMQ_RCVMORE, &more_recv, &s);
      *_more = (more_recv == 1) ? true : false;
    }
    return len;
  }

  /*
   * return number of bytes received, negative if error
   */
  int32_t RecvMsg(zmq::socket_t &sock, int32_t &cid,
      boost::shared_array<uint8_t> &data, bool *_more){
    zmq::message_t msgt;
    try{
      sock.recv(&msgt);
    }catch(zmq::error_t &e){
      LOG(ERROR) << "RecvMsg error = " << e.what();
      return -1;
    }

    size_t len = msgt.size();
    if(len != sizeof(int32_t)){
      LOG(ERROR) << "cid len != sizeof(int32_t) " 
	     << " len = " << len;

      return -1;
    }
    cid = *((int32_t *) msgt.data());

    return RecvMsg(sock, data, _more);
  }

  /*
   * return number of bytes sent, negative if error
   */
  int32_t SendMsg(zmq::socket_t &sock, const uint8_t *data, size_t len, int flag){
    int nbytes;
    try{
      VLOG(3) << "SendMsg single data, data = " << (void *) data 
		<< " len = " << len << " flag = " << flag;
      nbytes = sock.send(data, len, flag);
    }catch(zmq::error_t &e){
      VLOG(2) << "Send failed, e.what() = " << e.what();
      return -1;
    }
    VLOG(3) << "SendMsg single data end";
    return nbytes;
  }

  int32_t SendMsg(zmq::socket_t &sock, int32_t cid, const uint8_t *data, 
		  size_t len, int flag){
  
    int nbytes;
    try{
      VLOG(3) << "sending cid";
      nbytes = SendMsg(sock, (uint8_t *) &cid, sizeof(int32_t), 
		       flag | ZMQ_SNDMORE);
    }catch(zmq::error_t &e){
      VLOG(2) << "failed to send e.what() = " << e.what();
      return -1;
    }

    if(nbytes < 0) return -1;
    
    VLOG(3) << "Sent cid, size = " << nbytes;
    
    if(nbytes != sizeof(int32_t)){
      LOG(WARNING) << "send cid bytes num does not match, nbytes = " 
		<< nbytes;
      return -1;
    }
    return SendMsg(sock, data, len, flag);
  }

  // 0 for received nothing
  int32_t RecvMsgAsync(zmq::socket_t &sock, boost::shared_array<uint8_t> &data,
      bool *_more){
    zmq::message_t msgt;
    int nbytes;
    try{
      nbytes = sock.recv(&msgt, ZMQ_DONTWAIT);
    }catch(zmq::error_t &e){
      return -1;
    }

    if(nbytes == 0){
      if(zmq_errno() == EAGAIN)
        return 0;
      else
        return -1;
    }

    size_t len = msgt.size();
    uint8_t *dataptr;
    try{
      dataptr = new uint8_t[len];
    }catch(std::bad_alloc &e){
      return -1;
    }
    memcpy(dataptr, msgt.data(), len);
    data.reset(dataptr);

    if(_more != NULL){
      int more_recv;
      size_t s = sizeof(int);
      sock.getsockopt(ZMQ_RCVMORE, &more_recv, &s);
      *_more = (more_recv == 1) ? true : false;
    }

    return len;
  }

  // 0 for received nothing
  int32_t RecvMsgAsync(zmq::socket_t &sock, int32_t &cid,
      boost::shared_array<uint8_t> &data, bool *_more){
    zmq::message_t msgt;
    int nbytes;
    try{
      nbytes = sock.recv(&msgt, ZMQ_DONTWAIT);
    }catch(zmq::error_t &e){
      return -1;
    }
    if(nbytes == 0){
      if(zmq_errno() == EAGAIN)
        return 0;
      else
        return -1;
    }
    size_t len = msgt.size();
    if(len != sizeof(int32_t)) return -1;
    cid = *((int32_t *) msgt.data());

    return RecvMsg(sock, data, _more); 
    //it does not matter to use recv_msg_async or recv_msg
  }
}
