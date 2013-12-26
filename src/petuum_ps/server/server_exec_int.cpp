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

// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2013.12.21

#include <petuum_ps/proxy/server_proxy.hpp>
#include <petuum_ps/storage/dense_row.hpp>
#include <petuum_ps/comm_handler/comm_handler.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <petuum_ps/util/utils.hpp>

DEFINE_string(serverfile, "machinefile", "a list of servers");
DEFINE_int32(serverid, 0, "server id");
DEFINE_int32(num_clients, 1, "number of clients expected");

int main(int argc, char *argv[]){
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  int32_t myid = FLAGS_serverid;
  int32_t num_clients = FLAGS_num_clients;

  VLOG(0) << "server started, parameters: " << "\n"
    << "server id = " << myid << "\n"
    << "number expected clients = " << num_clients;

  std::vector<petuum::ServerInfo> serverinfos 
    = petuum::GetServerInfos(FLAGS_serverfile);

  petuum::ServerInfo myinfo;
  bool found = false;
  // based on my id, figure out my ip and port
  for(int i = 0; i < serverinfos.size(); ++i){
    if (serverinfos[i].id_ == myid) {
      myinfo = serverinfos[i];
      found = true;
      break;
    }
  }
  CHECK(found);
  VLOG(0) << "ip = " << myinfo.ip_ << "\n" << "port = " << myinfo.port_;

  // create zmq context
  zmq::context_t zmq_ctx(1);

  // CommHandler config parameters
  petuum::ConfigParam comm_config(myid, true, myinfo.ip_, myinfo.port_);
  petuum::CommHandler *comm = new petuum::CommHandler(comm_config);

  int suc = comm->Init(&zmq_ctx);
  CHECK_EQ(0, suc);

  petuum::ServerProxy<int> *proxy = new petuum::ServerProxy<int>(comm);

  bool is_namenode = (myid == serverinfos[0].id_);
  if (is_namenode) {
    suc = proxy->InitNameNode(serverinfos.size() - 1, num_clients);
    CHECK_EQ(0, suc) << "InitNameNode Failed";
  } else {
    // Supply namenode ip and port # to initialize non-namenode.
    suc = proxy->InitNonNameNode(serverinfos[0].id_, serverinfos[0].ip_, 
				 serverinfos[0].port_, num_clients);
    CHECK_EQ(0, suc);
  }
  proxy->StartProxy();

  VLOG(0) << "calling comm->ShutDown()";
  comm->ShutDown();
  //TODO(jinliang): where to shut down comm?
  delete comm;
  delete proxy;
};
