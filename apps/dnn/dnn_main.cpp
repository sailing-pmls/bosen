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
// Deep Neural Network built on Petuum Parameter Server
// Author: Pengtao Xie (pengtaox@cs.cmu.edu)



#include <gflags/gflags.h>
#include <glog/logging.h>
#include <iostream>
#include <string>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include "paras.h"
#include "dnn.h"


DEFINE_string(hostfile, "", "Path to file containing server ip:port.");
DEFINE_string(parafile,"", "Path to file containing DNN hyperparameters");
DEFINE_string(datafile,"","Path to file containing training data");
DEFINE_string(modelfile,"","Path to file to save DNN model");
DEFINE_int32(num_clients, 1, "Total number of clients");
DEFINE_int32(client_id, 0, "This client's ID");
DEFINE_int32(num_worker_threads,1,"Number of worker threads");
DEFINE_int32(staleness, 0, "Staleness");


// Main function
int main(int argc, char *argv[]) {

  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  //load dnn parameters
  dnn_paras para;
  load_dnn_paras(para, FLAGS_parafile.c_str());
  
  std::cout<<"client "<<FLAGS_client_id<< " starts working..."<<std::endl;
    
  // Configure Petuum PS
  petuum::TableGroupConfig table_group_config;
  // Global params
  table_group_config.num_total_server_threads = FLAGS_num_clients;  // 1 server thread per client
  table_group_config.num_total_bg_threads = FLAGS_num_clients;  // 1 background thread per client
  table_group_config.num_total_clients = FLAGS_num_clients;
  table_group_config.num_tables = (para.num_layers-1)*2;  // tables storing weights and biases
  
  // Local params
  table_group_config.num_local_server_threads = 1;
  table_group_config.num_local_bg_threads = 1;
  table_group_config.num_local_app_threads = FLAGS_num_worker_threads + 1;  // +1 for main() thread
  petuum::GetHostInfos(FLAGS_hostfile, &table_group_config.host_map);
  petuum::GetServerIDsFromHostMap(&table_group_config.server_ids, table_group_config.host_map);
  table_group_config.client_id = FLAGS_client_id;
  table_group_config.consistency_model = petuum::SSPPush;
  
  
  // Configure Petuum PS tables
  petuum::TableGroup::RegisterRow<petuum::DenseRow<float> >(0);  // Register dense rows as ID 0
  
  petuum::TableGroup::Init(table_group_config, false);  // Initializing thread does not need table access
  // Common table settings
  petuum::ClientTableConfig table_config;
  table_config.table_info.row_type = 0; // Dense rows
  table_config.oplog_capacity = 100;
  
  //create weight tables
  for(int i=0;i<para.num_layers-1;i++){
    table_config.table_info.table_staleness = FLAGS_staleness;
    table_config.table_info.row_capacity = para.num_units_ineach_layer[i];
    table_config.process_cache_capacity = para.num_units_ineach_layer[i+1];
    petuum::TableGroup::CreateTable(i,table_config);
  }

  //create biases tables
  for(int i=0;i<para.num_layers-1;i++){
    table_config.table_info.table_staleness = FLAGS_staleness;
    table_config.table_info.row_capacity = para.num_units_ineach_layer[i+1];
    table_config.process_cache_capacity = 1;
    petuum::TableGroup::CreateTable(i+para.num_layers-1,table_config);
  }
  
  
  // Finished creating tables
  petuum::TableGroup::CreateTableDone();
  
  //run dnn
  if(FLAGS_client_id==0)
    std::cout<<"DNN training starts ..."<<std::endl;
  dnn mydnn(para,FLAGS_client_id, FLAGS_num_worker_threads, FLAGS_staleness, FLAGS_modelfile);
  mydnn.load_data(FLAGS_datafile);
  boost::thread_group worker_threads;
  for (int i = 0; i < FLAGS_num_worker_threads; ++i) {
    	worker_threads.create_thread(
       boost::bind(&dnn::run, boost::ref(mydnn)));
  }
  petuum::TableGroup::WaitThreadRegister();
  worker_threads.join_all();
  
  if(FLAGS_client_id==0)
    std::cout<<"DNN training ends."<<std::endl;
  
  // Cleanup and output runtime
  petuum::TableGroup::ShutDown();
  
  return 0;
}
