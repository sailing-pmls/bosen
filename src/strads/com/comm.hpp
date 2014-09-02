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
/********************************************************
  @author: Jin Kyu Kim (jinkyuk@cs.cmu.edu)
  @desc: Warpper functions for the communication primitives 
         It provides common abstractions on top of 
         various communication library - MPI, ZMQ, RDMA

********************************************************/
#pragma once 

#include <ifaddrs.h>
#include <netinet/in.h> 
#include <string.h> 
#include <arpa/inet.h>
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

#define USER_MSG_SIZE (16*1024-72)
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
  USER_PROGRESS_CHECK,
  SYSTEM_DSHARD_END
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

#include "./include/common.hpp"
#include "./com/zmq/zmq-common.hpp"

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


