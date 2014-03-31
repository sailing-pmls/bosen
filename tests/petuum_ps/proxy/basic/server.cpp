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

#include <petuum_ps/proxy/server_proxy.hpp>
#include <petuum_ps/storage/dense_row.hpp>
#include <petuum_ps/comm_handler/comm_handler.hpp>

#include <pthread.h>

#include <petuum_ps/util/utils.hpp>

int main(int argc, char *argv[]){

  boost_po::options_description options("Allowed options");
  int32_t id;
  std::string server_file;
  int32_t nclis;
  
  options.add_options()
    ("id", boost_po::value<int32_t>(&id)->default_value(0), "node id")
    ("servers",
    boost_po::value<std::string>(&server_file)->default_value("servers.txt"))
    ("nclis", boost_po::value<int32_t>(&nclis)->default_value(1));
  
  boost_po::variables_map options_map;
  boost_po::store(boost_po::parse_command_line(argc, argv, options), options_map);
  boost_po::notify(options_map);  
  google::InitGoogleLogging(argv[0]);

  std::vector<petuum::ServerInfo> serverinfos = petuum::GetServerInfos(server_file);

  petuum::ServerInfo myinfo;
  
  for(int i = 0; i < serverinfos.size(); ++i){
    if(serverinfos[i].id_ == id){
      myinfo = serverinfos[i];
      break;
    }
  }

  zmq::context_t zmq_ctx(1);
    
  petuum::ConfigParam comm_config(id, true, myinfo.ip_, myinfo.port_);
  
  petuum::CommHandler *comm = new petuum::CommHandler(comm_config);
  
  int suc = comm->Init(&zmq_ctx);
  assert(suc == 0);

  petuum::ServerProxy<petuum::DenseRow, int> *proxy 
    = new petuum::ServerProxy<petuum::DenseRow, int>(*comm);

  petuum::ServerConfigParam server_config(id, nclis, serverinfos.size() , 
					  serverinfos[0].id_, serverinfos[0].ip_,
					  serverinfos[0].port_);

  petuum::DistributedServer<petuum::DenseRow, int> *dist_server
     = new petuum::DistributedServer<petuum::DenseRow, int>(server_config, *proxy);
  
  LOG(INFO) << "here!!";
  pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

  petuum::ServerThreadInfo<petuum::DenseRow, int> thrinfo;
  thrinfo.comm_ = comm;
  thrinfo.server_ = dist_server;
  thrinfo.mtx_ = &mtx;
  thrinfo.is_namenode_ = (id == serverinfos[0].id_);


  LOG(INFO) << "create pthread";
  pthread_t thr;
  suc = pthread_create(&thr, NULL, 
		       petuum::ServerProxy<petuum::DenseRow, int>::ThreadMain,
		       &thrinfo);
  assert(suc == 0);
  pthread_join(thr, NULL);
  
  //TODO(jinliang): where to shut down comm?
  delete comm;
  delete proxy;
  delete dist_server;
};
