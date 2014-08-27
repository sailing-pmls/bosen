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

#include <ifaddrs.h>
#include <netinet/in.h> 
#include <string.h> 
#include <arpa/inet.h>
#include <map>
#include <string>
#include <fstream>
#include <iostream>
#include <istream>
#include <stdio.h>

class mnode;
class mlink;
class stnode;

#include "./include/common.hpp"
#include "./include/strads-macro.hpp"
#include "./include/utility.hpp"

enum port_type { in, out };
enum handler_type { ZMQ, RDMA };
enum link_type { full, hanlf };
enum node_type { passive, active, bg }; 
                                  
#define IPSTRLEN INET_ADDRSTRLEN
#define MAX_IN_PORT  (128)
#define MAX_OUT_PORT (128)
#define MAX_FUNC_NAME (256)

class mnode{
public:
  std::string ip;
  void *(*func)(void *);
  std::string funcname;
  int id;
};

class mlink{
public:
  int srcnode;
  int srcport;
  int dstnode;
  int dstport;
  int id;
};

class sthandler_wrapper {
public:
  void *handler; // native handler. ZMQ-based com handler library or RDMA-based library 
  handler_type type;
};

class stlink {
public:
  link_type type; // half or fulll
  std::string dst_ip;
  uint16_t dst_port;
  std::string src_ip;
  uint16_t src_port;
  // if full duplex, src/dst has no difference 
};

class stport {
  char dst_ip[IPSTRLEN];
  char src_ip[IPSTRLEN];
  uint16_t dst_port;
  uint16_t src_port;
  stlink *link;
  sthandler_wrapper *handler; // high level abstraction for network handler 
};

class strangectx {
public:
  unsigned long int row_start;
  unsigned long int row_end;
  unsigned long int col_start;
  unsigned long int col_end;
};

class stparts {
public:
  strangectx maxrange;
  strangectx prange;
};

class stnode {
public:
  int node_id;
  char ipstr[IPSTRLEN]; // machine associated IP address 

  std::map<int, stport*>inports;
  std::map<int, stport*>outports;

  mnode *routine;
  std::map<int, stparts*>d_parts; // data partition
  std::map<int, stparts*>m_parts; // model partition

}; // a process node  

void parse_nodefile(std::string &fn, sharedctx &shctx);
void parse_linkfile(std::string &fn, sharedctx &shctx);
void parse_rlinkfile(std::string &fn, sharedctx &shctx);
void parse_starlinkfile(std::string &fn, sharedctx &shctx);
