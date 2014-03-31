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

#include <boost/program_options.hpp>
namespace boost_po = boost::program_options;

#include <string>
#include <stdio.h>
#include <glog/logging.h>
#include <stdint.h>
#include <stdlib.h>

#include <petuum_ps/consistency/op_log_manager.hpp>
#include <petuum_ps/proxy/client_proxy.hpp>
#include <petuum_ps/comm_handler/comm_handler.hpp>
#include <petuum_ps/storage/thread_safe_lru_row_storage.hpp>
#include <petuum_ps/storage/dense_row.hpp>
#include <petuum_ps/consistency/consistency_controller.hpp>

#include <vector>
#include <fstream>
#include <zmq.hpp>

#include <petuum_ps/util/utils.hpp>

int main(int argc, char *argv[]){
  boost_po::options_description options("Allowed options");
  int32_t id;
  std::string server_file;
  
  options.add_options()
    ("id", boost_po::value<int32_t>(&id)->default_value(1), "node id")
    ("servers",
    boost_po::value<std::string>(&server_file)->default_value("servers.txt"));
  
  boost_po::variables_map options_map;
  boost_po::store(boost_po::parse_command_line(argc, argv, options), options_map);
  boost_po::notify(options_map);  
  google::InitGoogleLogging(argv[0]);

  std::vector<petuum::ServerInfo> serverinfos = petuum::GetServerInfos(server_file);
  
  petuum::ClientProxyConfig proxyconfig;
  proxyconfig.servers_ = serverinfos;
  
  petuum::CommHandler *comm;
  petuum::ConfigParam config(id, false, "", "");
  
  try{
    comm = new petuum::CommHandler(config);
  }catch(...){
    LOG(INFO) << "failed to create comm\n";
    return -1;
  }

  zmq::context_t zmq_ctx(1);
  int suc = comm->Init(&zmq_ctx);

  if(suc == 0) LOG(INFO) << "comm_handler init succeeded" << std::endl;
  else{
    LOG(ERROR) << "comm_handler init failed" << std::endl;
    return -1;
  }

  petuum::ClientProxy<int> proxy(*comm, proxyconfig);
  int32_t table_id = 1;
  suc = proxy.Initialize();
  assert(suc == 0);
  
  LOG(INFO) << "proxy.Initialize() succeeds";

  suc = proxy.RegisterThread();
  assert(suc == 0);

  petuum::SspPolicy<int> ssp_policy(0);

  petuum::StorageConfig storage_config;
  storage_config.max_size = 100;

  // OpLogManagerConfig
  petuum::OpLogManagerConfig op_log_config;
  op_log_config.thread_cache_capacity = 100;
  op_log_config.max_pending_op_logs = 100;

  petuum::TableConfig table_config;
  table_config.process_storage_config = storage_config;
  table_config.op_log_config = op_log_config;
  table_config.server_storage_config = storage_config;
  table_config.num_columns = 100;

  suc = proxy.RequestCreateTableRPC(table_id, table_config);
  assert(suc == 0);
  
  LOG(INFO) << "********************* CreateTableRPC success *******************";

  // ConsistencyControllerConfig.
  petuum::ConsistencyControllerConfig<int> controller_config;
  controller_config.table_id = table_id;
  controller_config.storage_config = storage_config;
  controller_config.policy = &ssp_policy;
  controller_config.proxy = &proxy;
  controller_config.op_log_config = op_log_config;
  
  petuum::ConsistencyController<petuum::DenseRow, int> controller(controller_config);

  // iteration 0
  controller.DoInc(1, 5, 10);
  LOG(INFO) << "controller.DoInc(1, 5, 10) done";
  int pa;
  //suc = controller.DoGet(1, 5, &pa);
  //std::cout << "TABLE(1, 5) = " << pa << std::endl;

  controller.DoInc(2, 2, 1);

  suc = controller.DoGet(2, 3, &pa);
  std::cout << "TABLE(2, 3) = " << pa << std::endl;

  controller.DoIterate();
  
  LOG(INFO) << "****************Iteration 0 done***************************";

  // iteration 1
  controller.DoInc(3, 1, 2);
  controller.DoInc(2, 3, 1);

  suc = controller.DoGet(2, 3, &pa);
  std::cout << "TABLE(2, 3) = " << pa << std::endl;

  controller.DoInc(2, 2, 1);
  controller.DoIterate();
  LOG(INFO) << "****************Iteration 1 done***************************";

  
  //iteration 2
  int a, b, c;
  suc = controller.DoGet(1, 5, &a);
  assert(suc == 0);
  suc = controller.DoGet(2, 3, &b);
  assert(suc == 0);
  suc = controller.DoGet(2, 2, &c);
  assert(suc == 0);

  std::cout << "TABLE(1, 5) = " << a << "\n"
	    << "TABLE(2, 3) = " << b << "\n"
	    << "TABLE(2, 2) = " << c << std::endl;

  assert(a == 10);
  assert(b == 1);
  assert(c == 2);

  LOG(INFO) << "****************Iteration 2 done***************************";
  
  suc = proxy.DeregisterThread();
  comm->ShutDown();
  delete comm;
  std::cout << "TEST PASSED!" << std::endl;
  return 0;
}
