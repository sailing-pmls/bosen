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

#include "zmq_util.hpp"

// 0 for received nothing
int recv_msg_async(zmq::socket_t &sock, boost::shared_array<uint8_t> &data)
{
    zmq::message_t msgt;
    int nbytes;
    try{
      nbytes = sock.recv(&msgt, ZMQ_DONTWAIT);
    }catch(zmq::error_t e){
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
    }catch(std::bad_alloc e){
      return -1;
    }
    
    memcpy(dataptr, msgt.data(), len);
    data.reset(dataptr);
    return len;
}
// 0 for received nothing
int recv_msg_async(zmq::socket_t &sock, commtest::cliid_t &cid, boost::shared_array<uint8_t> &data)
{
    zmq::message_t msgt;
    int nbytes;
    try{
      nbytes = sock.recv(&msgt, ZMQ_DONTWAIT);
    }catch(zmq::error_t e){
      return -1;
    }

    if(nbytes == 0){
      if(zmq_errno() == EAGAIN)
	return 0;
      else
	return -1;
    }
    
    size_t len = msgt.size();
    if(len != sizeof(commtest::cliid_t)) return -1;

    cid = *((commtest::cliid_t *) msgt.data());

    return recv_msg(sock, data); //it does not matter to use recv_msg_async or recv_msg
}

/*
 * return number of bytes received, negative if error, 0 for received nothing, which should be treated as error
 */
int recv_msg(zmq::socket_t &sock, boost::shared_array<uint8_t> &data)
{
    zmq::message_t msgt;
    int nbytes;
    try{
      nbytes = sock.recv(&msgt);
    }catch(zmq::error_t e){
      //LOG(NOR, stderr, "recv failed: %s\n", e.what());
      return -1;
    }

    if(nbytes == 0) return 0;

    size_t len = msgt.size();    
    uint8_t *dataptr;
    try{
      dataptr = new uint8_t[len];
    }catch(std::bad_alloc e){
      //LOG(DBG, stderr, "can not allocate memory!\n");
      return -1;
    }
    
    memcpy(dataptr, msgt.data(), len);
    data.reset(dataptr);
    return len;
}

/*
 * return number of bytes received, negative if error
 */
int recv_msg(zmq::socket_t &sock, commtest::cliid_t &cid, boost::shared_array<uint8_t> &data)
{
    zmq::message_t msgt;
    try{
      sock.recv(&msgt);
    }catch(zmq::error_t e){
      //LOG(NOR, stderr, "recv cid failed: %s\n", e.what());
      return -1;
    }

    size_t len = msgt.size();
    if(len != sizeof(commtest::cliid_t)) return -1;

    cid = *((commtest::cliid_t *) msgt.data());
    //LOG(DBG, stderr, "received id = 0x%x\n", cid);

    return recv_msg(sock, data);
}

/*
 * return number of bytes sent, negative if error
 */
int send_msg(zmq::socket_t &sock, uint8_t *data, size_t len, int flag){
  
  int nbytes;
  try{
    nbytes = sock.send(data, len, flag);
  }catch(zmq::error_t e){
    //LOG(NOR, stderr, "send failed: %s\n", e.what());
    return -1;
  }
  return nbytes;
}

int send_msg(zmq::socket_t &sock, commtest::cliid_t cid, uint8_t *data, size_t len, int flag)
{
  
  //LOG(DBG, stderr, "send to cid = 0x%x\n", cid);
  int nbytes;
  try{    
    nbytes = send_msg(sock, (uint8_t *) &cid, sizeof(commtest::cliid_t), flag | ZMQ_SNDMORE);
  }catch(zmq::error_t e){
    //LOG(NOR, stderr, "send cid failed: %s\n", e.what());
    return -1;
  }

  if(nbytes != sizeof(commtest::cliid_t)){
    //LOG(NOR, stderr, "send client id failed, sent %d bytes\n", nbytes); 
    return -1;
  }
  return send_msg(sock, data, len, flag);
}
