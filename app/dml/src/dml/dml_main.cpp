

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <iostream>
#include <string>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <petuum_ps_common/include/configs.hpp>
#include "dml_paras.hpp"
#include "dml.hpp"
#include <stdio.h>
#include <stdlib.h>
#include "utils.hpp"

DEFINE_string(hostfile, "", "Path to file containing server ip:port.");
DEFINE_string(parafile, "", "Path to file containing DNN hyperparameters");
DEFINE_int32(num_clients, 1, "Total number of clients");
DEFINE_int32(client_id, 0, "This client's ID");
DEFINE_int32(num_worker_threads, 1, "Number of worker threads");
DEFINE_int32(staleness, 0, "Staleness");
DEFINE_int32(snapshot_clock, 0, "How often to take PS snapshots; 0 disables");
DEFINE_int32(resume_clock, 0, "If nonzero, resume from the PS snapshot with this ID");
DEFINE_string(snapshot_dir, "", "Path to save PS snapshots");
DEFINE_string(resume_dir, "", "Where to retrieve PS snapshots for resuming");
DEFINE_string(model_weight_file, "", "file to save model");
DEFINE_int32(ps_row_in_memory_limit, 1000, \
         "Single-machine version only: max # rows of weight matrices that can be held in memory");
DEFINE_string(feature_file,"","");
DEFINE_string(simi_pairs_file,"","");
DEFINE_string(diff_pairs_file,"","");
// Main function
int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  // load dnn parameters
  // configure meta
  DmlParas dmlprs;
  LoadDmlParas(&dmlprs, FLAGS_parafile.c_str());
  std::cout << "client " << FLAGS_client_id << " starts working..." << std::endl;
  // Configure Petuum PS
  petuum::TableGroupConfig table_group_config;
  // Global params
  table_group_config.num_total_clients = FLAGS_num_clients;
  table_group_config.num_tables = 1;  // tables storing weights and biases
  // Single-node-PS versus regular distributed PS
#ifdef PETUUM_SINGLE_NODE
  table_group_config.ooc_path_prefix = "mlr_sn.localooc";
  table_group_config.consistency_model = petuum::LocalOOC;
#else
  petuum::GetHostInfos(FLAGS_hostfile, &table_group_config.host_map);
  table_group_config.consistency_model = petuum::SSPPush;
#endif
  // Local parameters for this process
  table_group_config.num_local_app_threads = FLAGS_num_worker_threads + 1;  // +1 for main() thread
  table_group_config.client_id = FLAGS_client_id;
  // snapshots
  table_group_config.snapshot_clock = FLAGS_snapshot_clock;
  table_group_config.resume_clock = FLAGS_resume_clock;
  table_group_config.snapshot_dir = FLAGS_snapshot_dir;
  table_group_config.resume_dir = FLAGS_resume_dir;
  // Configure Petuum PS tables
  petuum::PSTableGroup::RegisterRow<petuum::DenseRow<float> >(0);  // Register dense rows as ID 0
  // Initializing thread does not need table access
  petuum::PSTableGroup::Init(table_group_config, false);
  // Common table settings
  petuum::ClientTableConfig table_config;
  table_config.table_info.row_type = 0;  // Dense rows
  table_config.oplog_capacity = 100;
  //  create weight tables
  table_config.table_info.table_staleness = FLAGS_staleness;
  table_config.table_info.row_capacity = dmlprs.src_feat_dim;
  //  table_config.table_info.row_oplog_type=kDenseRowOpLog;
#ifdef PETUUM_SINGLE_NODE
    table_config.process_cache_capacity = std::min(FLAGS_ps_row_in_memory_limit, \
           dmlprs.dst_feat_dim);
#else
    table_config.process_cache_capacity = dmlprs.dst_feat_dim;
#endif
    petuum::PSTableGroup::CreateTable(0, table_config);
  // Finished creating tables
  petuum::PSTableGroup::CreateTableDone();
  // load data
  DML mydml(dmlprs.src_feat_dim, dmlprs.dst_feat_dim, dmlprs.lambda, dmlprs.thre, \
           FLAGS_client_id, FLAGS_num_worker_threads, \
           dmlprs.num_smps_evaluate, FLAGS_staleness, \
           FLAGS_num_clients, dmlprs.size_mb, dmlprs.num_iters_evaluate );
  mydml.LoadData(dmlprs.num_total_pts,  dmlprs.num_simi_pairs, dmlprs.num_diff_pairs, \
                FLAGS_feature_file.c_str(), FLAGS_simi_pairs_file.c_str(),  FLAGS_diff_pairs_file.c_str());
  boost::thread_group worker_threads;
  std::srand(time(NULL));
  for (int i = 0; i < FLAGS_num_worker_threads; ++i) {
    worker_threads.create_thread(
    boost::bind(&DML::Learn, boost::ref(mydml), dmlprs.learn_rate, dmlprs.epoch, FLAGS_model_weight_file.c_str()));
  }
  worker_threads.join_all();
  
  // Cleanup and output runtime
  petuum::PSTableGroup::ShutDown();
  std::cout<<"DML training ends"<<std::endl;
  return 0;
}
