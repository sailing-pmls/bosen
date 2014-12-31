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
#include "mlr_paras.h"
#include "mlr.h"
#include <stdio.h>
#include <stdlib.h>
#include "util.h"

DEFINE_string(hostfile, "", "Path to file containing server ip:port.");
DEFINE_string(parafile,"", "Path to file containing DNN hyperparameters");
DEFINE_int32(num_clients, 1, "Total number of clients");
DEFINE_int32(num_comm_channels_per_client, 1,
    "number of comm channels per client");
DEFINE_int32(client_id, 0, "This client's ID");
DEFINE_int32(num_worker_threads, 1, "Number of worker threads");
DEFINE_int32(staleness, 0, "Staleness");
DEFINE_string(data_ptt_file, "", "Path to file containing the number of data "
    "points in each partition");
DEFINE_int32(snapshot_clock, 0, "How often to take PS snapshots; 0 disables");
DEFINE_int32(resume_clock, 0, "If nonzero, resume from the PS snapshot with this ID");
DEFINE_string(model_weight_file, "", "Path to save weight matrix");
DEFINE_string(snapshot_dir, "", "Path to save PS snapshots");
DEFINE_string(resume_dir, "", "Where to retrieve PS snapshots for resuming");

const int32_t kDenseRowFloatTypeID = 0;

// Main function
int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  //load dnn parameters
  //configure meta
  mlr_paras mlrprs;
  load_mlr_paras(mlrprs, FLAGS_parafile.c_str());

  std::cout<<"client "<<FLAGS_client_id<< " starts working..."<<std::endl;

  // Configure Petuum PS
  petuum::TableGroupConfig table_group_config;
  // Global params
  table_group_config.num_comm_channels_per_client
    = FLAGS_num_comm_channels_per_client;
  table_group_config.num_total_clients = FLAGS_num_clients;
  table_group_config.num_tables = 1;  // tables storing weights and biases

  // Single-node-PS versus regular distributed PS
#ifdef PETUUM_SINGLE_NODE
  table_group_config.ooc_path_prefix = "mlr_sn.localooc";
  table_group_config.consistency_model = petuum::LocalOOC;
#else
  petuum::GetHostInfos(FLAGS_hostfile, &table_group_config.host_map);
  //petuum::GetServerIDsFromHostMap(&table_group_config.server_ids, table_group_config.host_map);
  table_group_config.consistency_model = petuum::SSPPush;
#endif

  // +1 for main() thread
  table_group_config.num_local_app_threads = FLAGS_num_worker_threads + 1; 
  table_group_config.client_id = FLAGS_client_id;
  // snapshots
  table_group_config.snapshot_clock = FLAGS_snapshot_clock;
  table_group_config.resume_clock = FLAGS_resume_clock;
  table_group_config.snapshot_dir = FLAGS_snapshot_dir;
  table_group_config.resume_dir = FLAGS_resume_dir; 

  // Configure Petuum PS tables
  // Register dense rows as ID 0
  petuum::PSTableGroup::RegisterRow<petuum::DenseRow<float> >(kDenseRowFloatTypeID);

  // Initializing thread does not need table access
  petuum::PSTableGroup::Init(table_group_config, false);
  // Common table settings
  petuum::ClientTableConfig table_config;
  table_config.table_info.row_type = kDenseRowFloatTypeID;
  table_config.oplog_capacity = 100;

  // create weight tables
  table_config.table_info.table_staleness = FLAGS_staleness;
  table_config.table_info.row_capacity = mlrprs.dim_feat;
#ifdef PETUUM_SINGLE_NODE
  table_config.process_cache_capacity = std::min(FLAGS_ps_row_in_memory_limit, mlrprs.num_classes);
#else    
  table_config.process_cache_capacity = mlrprs.num_classes;
#endif
  petuum::PSTableGroup::CreateTable(0,table_config);

  // Finished creating tables
  petuum::PSTableGroup::CreateTableDone();

  //load data
  int num_data=0;
  char feats_file[512];
  char label_file[512];
  load_data_meta(mlrprs.data_file_list, feats_file, label_file,num_data);
  float ** input_feature= new float *[num_data];
  for (int k = 0; k < num_data; k++)
    input_feature[k]=new float[mlrprs.dim_feat];
  load_input_data(input_feature, num_data, mlrprs.dim_feat, feats_file);
  //simulate_feature(input_feature, num_data, mlrprs.dim_feat);
  if (FLAGS_client_id == 0) {
    std::cout<<"check features: " << std::endl;
    check_feature(input_feature, num_data, mlrprs.dim_feat);
  }
  //output labels
  //std::cout<<"num_data: "<<num_data;
  float * output_labels=new float [num_data];
  load_output_labels(output_labels, num_data,  label_file);
  //simulate_labels(output_labels, num_data, mlrprs.num_classes);
  if(FLAGS_client_id==0){
    std::cout<<"check labels: "<<std::endl;
    check_label(output_labels,num_data);
  }

  //load evaluation data

  int num_eval_data=0;
  char eval_feats_file[512];
  char eval_label_file[512];
  load_data_meta(mlrprs.eval_data_meta, eval_feats_file, eval_label_file,num_eval_data);
  float ** eval_feature=new float *[num_eval_data];
  for(int k=0;k<num_eval_data;k++)
    eval_feature[k]=new float[mlrprs.dim_feat];


  load_input_data(eval_feature, num_eval_data, mlrprs.dim_feat, eval_feats_file);
  //simulate_feature(input_feature, num_data, mlrprs.dim_feat);

  if(FLAGS_client_id==0){
    std::cout<<"worker "<<FLAGS_client_id<<" check eval features: "<<std::endl;
    check_feature(eval_feature, num_eval_data, mlrprs.dim_feat);
  }


  //output labels
  //std::cout<<"num_data: "<<num_data;
  float * eval_labels=new float [num_eval_data];
  load_output_labels(eval_labels, num_eval_data,  eval_label_file);
  //simulate_labels(output_labels, num_data, mlrprs.num_classes);
  if(FLAGS_client_id==0){
    std::cout<<"worker "<<FLAGS_client_id<<" check eval labels: "<<std::endl;
    check_label(eval_labels,num_eval_data);
  }


  mlr mymlr(mlrprs, FLAGS_client_id, FLAGS_num_worker_threads, FLAGS_staleness);


  //std::cout<<"client "<<FLAGS_client_id<< " is still good "<<std::endl;

  boost::thread_group worker_threads;
  for (int i = 0; i < FLAGS_num_worker_threads; ++i) {
    worker_threads.create_thread(
        boost::bind(&mlr::compute_ss, boost::ref(mymlr), input_feature, output_labels,num_data, eval_feature, eval_labels, num_eval_data));
  }
  petuum::PSTableGroup::WaitThreadRegister();
  worker_threads.join_all();

  // Cleanup and output runtime
  petuum::PSTableGroup::ShutDown();

  return 0;
}
