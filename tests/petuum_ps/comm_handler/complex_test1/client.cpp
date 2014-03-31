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
#include <boost/shared_array.hpp>
#include <boost/program_options.hpp>
namespace boost_po = boost::program_options;

#include <string>
#include <comm_handler.hpp>
#include <stdio.h>
#include <glog/logging.h>
#include <pthread.h>

using namespace petuum;

struct thrinfo{
  int32_t sid_; // server id
  CommHandler *comm_;  
  pthread_mutex_t *mtx_;
  
  pthread_mutex_t *sync_mtx_;
  pthread_cond_t *sync_cond_;
};

void *thr_main(void *ti){
  thrinfo *tinfo = (thrinfo *) ti;
  CommHandler *comm = tinfo->comm_;
  
  pthread_mutex_lock(tinfo->mtx_);
  int suc = comm->RegisterThr();
  pthread_mutex_unlock(tinfo->mtx_);

  if(suc < 0)
    LOG(FATAL) << "register thread failed";

  LOG(INFO) << "RegisterThr() succeeds";

  int32_t sid;
  boost::shared_array<uint8_t> data;

  suc = comm->Recv(sid, data);
  assert(suc > 0);
  
  LOG(INFO) << "comm->Recv = " << (char *) data.get();

  pthread_t tid = pthread_self();
  suc = comm->Send(tinfo->sid_, (uint8_t *) &tid, sizeof(pthread_t), 0);
  assert(suc == sizeof(pthread_t));

  suc = comm->RecvThr(sid, data);

  assert(suc > 0);
  pthread_mutex_lock(tinfo->mtx_);
  std::cout << "thread = " << pthread_self()
	    << " received size = " << suc 
	    << " msg = " << (char *) data.get() << std::endl;
  pthread_mutex_unlock(tinfo->mtx_);

  pthread_mutex_lock(tinfo->mtx_);  
  comm->DeregisterThr();
  pthread_mutex_unlock(tinfo->mtx_);

  return 0;
}

int main(int argc, char *argv[]){
  boost_po::options_description options("Allowed options");
  std::string sip;
  std::string sport;
  int32_t id;
  int32_t sid;
  int32_t num_thrs;

  options.add_options()
    ("id", boost_po::value<int32_t>(&id)->default_value(1), "node id")
    ("sid", boost_po::value<int32_t>(&sid)->default_value(0), "scheduler id")
    ("sip", boost_po::value<std::string>(&sip)->default_value("127.0.0.1"), 
     "ip address")
    ("sport", boost_po::value<std::string>(&sport)->default_value("9999"), 
     "port number")
    ("nthrs", boost_po::value<int32_t>(&num_thrs)->default_value(2),
     "number of threads");
  
  boost_po::variables_map options_map;
  boost_po::store(boost_po::parse_command_line(argc, argv, options), options_map);
  boost_po::notify(options_map);  
  google::InitGoogleLogging(argv[0]);

  ConfigParam config(id, false, "", "");
  
  CommHandler *comm;
  try{
    comm = new CommHandler(config);
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

  comm->ConnectTo(sip, sport, sid);
  if(suc < 0){
    LOG(INFO) << "failed to connect to server\n";
    return 1;
  }

  boost::shared_array<uint8_t> data;
  int32_t rid;

  suc = comm->Recv(rid, data);
  assert(suc > 0 && rid == sid);

  printf("Received msg : %d from %d\n", *((int32_t *) data.get()), sid);
    
  thrinfo info;
  info.sid_ = sid;
  info.comm_ = comm;
  pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
  info.mtx_ = &mtx;
  
  pthread_t *thrs = new pthread_t[num_thrs];
  int i;
  for(i = 0; i < num_thrs; ++i){
    pthread_create(&(thrs[i]), NULL, thr_main, &info);
  }

  suc = comm->Send(sid, (uint8_t *) "hello", 6);
  assert(suc > 0);

  for(i = 0; i < num_thrs; ++i){
    pthread_join(thrs[i], NULL);
  }

  suc = comm->Send(sid, (uint8_t *) "hello again", 12);
  assert(suc > 0);
  
  delete[] thrs;

  std::cout << "TEST NEARLY PASSED!! SHUTTING DOWN COMM THREAD!!" << std::endl;
  suc = comm->ShutDown();
  if(suc < 0){
    LOG(ERROR) << "failed to shut down comm handler\n";
    return -1;
  }
  delete comm;
  std::cout << "TEST PASSED!! EXITING!!" << std::endl;
  return 0;
}
