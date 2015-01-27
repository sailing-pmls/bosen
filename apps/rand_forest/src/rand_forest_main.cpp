// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.11.06

#include "common.hpp"
#include "rand_forest_engine.hpp"
#include <petuum_ps_common/include/petuum_ps.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <thread>
#include <vector>
#include <cstdint>

// Petuum Parameters
DEFINE_string(hostfile, "", "Path to file containing server ip:port.");
DEFINE_int32(num_clients, 1, "Total number of clients");
DEFINE_int32(num_app_threads, 1, "Number of app threads in this client");
DEFINE_int32(client_id, 0, "Client ID");
DEFINE_string(consistency_model, "SSPPush", "SSP or SSPPush");
DEFINE_string(stats_path, "", "Statistics output file");
DEFINE_int32(num_comm_channels_per_client, 1,
    "number of comm channels per client");

// Data Parameters
DEFINE_int32(num_train_data, 0, "Number of training data. Cannot exceed the "
    "number of data in train_file. 0 to use all train data.");
DEFINE_string(train_file, "", "The program expects 2 files: train_file, "
    "train_file.meta. If global_data = false, then it looks for train_file.X, "
    "train_file.X.meta, where X is the client_id.");
DEFINE_bool(global_data, false, "If true, all workers read from the same "
    "train_file. If false, append X. See train_file.");
DEFINE_string(test_file, "", "The program expects 2 files: test_file, "
    "test_file.meta, test_file must have format specified in read_format "
    "flag. All clients read test file if FLAGS_perform_test == true.");
DEFINE_bool(perform_test, false, "Ignore test_file if false.");
DEFINE_bool(compute_importance, false, "If true, compute feature importance after building model.");


// Rand Forest Parameters
DEFINE_int32(num_trees, 1, "# of trees in the forest across all "
    "threads & workers.");
DEFINE_int32(max_depth, 1, "max depth of each decision tree.");
DEFINE_int32(num_data_subsample, 100, "# data used in determining each split");
DEFINE_int32(num_features_subsample, 3, "# of randomly selected features to "
    "consider for a split.");

// Misc
DEFINE_int32(num_tables, 2, "num PS tables.");
DEFINE_int32(test_vote_table_id, 1, "Vote table for test data.");
DEFINE_int32(gain_ratio_table_id, 2, "Gain ratio table.");
DEFINE_int32(row_oplog_type, petuum::RowOpLogType::kSparseRowOpLog,
    "row oplog type");
DEFINE_bool(oplog_dense_serialized, false, "True to not squeeze out the 0's "
    "in dense oplog. This is ignored for sparse oplog (which always "
    "squeeze out 0).");

const int32_t kDenseRowIntTypeID = 0;
const int32_t kDenseRowFloatTypeID = 1;

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  LOG(INFO) << "Starting Rand Forest with " << FLAGS_num_app_threads
    << " threads";

  tree::RandForestEngine rand_forest_engine;
  rand_forest_engine.ReadData();
  int num_labels = rand_forest_engine.GetNumLabels();
  int num_test_data = rand_forest_engine.GetNumTestData();
  int feature_dim = rand_forest_engine.GetNumFeatureDim();

  petuum::TableGroupConfig table_group_config;
  table_group_config.num_comm_channels_per_client
    = FLAGS_num_comm_channels_per_client;
  table_group_config.num_total_clients = FLAGS_num_clients;
  // a table to store votes from each tree and a table to store each nodes' gain ratio from all trees.
  table_group_config.num_tables = FLAGS_num_tables;
  // + 1 for main() thread.
  table_group_config.num_local_app_threads = FLAGS_num_app_threads + 1;
  table_group_config.client_id = FLAGS_client_id;
  table_group_config.stats_path = FLAGS_stats_path;

  petuum::GetHostInfos(FLAGS_hostfile, &table_group_config.host_map);
  if (std::string("SSP").compare(FLAGS_consistency_model) == 0) {
    table_group_config.consistency_model = petuum::SSP;
  } else if (std::string("SSPPush").compare(FLAGS_consistency_model) == 0) {
    table_group_config.consistency_model = petuum::SSPPush;
  } else if (std::string("LocalOOC").compare(FLAGS_consistency_model) == 0) {
    table_group_config.consistency_model = petuum::LocalOOC;
  } else {
    LOG(FATAL) << "Unkown consistency model: " << FLAGS_consistency_model;
  }

  petuum::PSTableGroup::RegisterRow<petuum::DenseRow<int> >
    (kDenseRowIntTypeID);
  petuum::PSTableGroup::RegisterRow<petuum::DenseRow<float> >
	(kDenseRowFloatTypeID);

  // Use false to disallow main thread to access table API.
  petuum::PSTableGroup::Init(table_group_config, false);

  // Create vote_table to collect tests.
  petuum::ClientTableConfig table_config;
  table_config.table_info.row_type = kDenseRowIntTypeID;
  table_config.table_info.table_staleness = 0;
  table_config.table_info.row_capacity = num_labels;
  table_config.table_info.row_oplog_type = FLAGS_row_oplog_type;
  table_config.table_info.oplog_dense_serialized =
    FLAGS_oplog_dense_serialized;
  // each test data is a row.
  table_config.process_cache_capacity = num_test_data;
  table_config.oplog_capacity = table_config.process_cache_capacity;
  petuum::PSTableGroup::CreateTable(FLAGS_test_vote_table_id, table_config);

  // Create gain_ratio_table to collect gain ratio
  petuum::ClientTableConfig gain_ratio_table_config;
  gain_ratio_table_config.table_info.row_type = kDenseRowFloatTypeID;
  gain_ratio_table_config.table_info.table_staleness = 0;
  gain_ratio_table_config.table_info.row_capacity = feature_dim; 
  gain_ratio_table_config.table_info.row_oplog_type = FLAGS_row_oplog_type;
  gain_ratio_table_config.table_info.oplog_dense_serialized =
    FLAGS_oplog_dense_serialized;
  gain_ratio_table_config.process_cache_capacity = 1; // only one row
  gain_ratio_table_config.oplog_capacity = gain_ratio_table_config.process_cache_capacity;
  petuum::PSTableGroup::CreateTable(FLAGS_gain_ratio_table_id, gain_ratio_table_config);

  // finish Create Tables
  petuum::PSTableGroup::CreateTableDone();

  LOG(INFO) << "Starting RF with " << FLAGS_num_app_threads << " threads "
    << "on client " << FLAGS_client_id;

  std::vector<std::thread> threads(FLAGS_num_app_threads);
  for (auto& thr : threads) {
    thr = std::thread(&tree::RandForestEngine::Start,
        std::ref(rand_forest_engine));
  }
  for (auto& thr : threads) {
    thr.join();
  }

  petuum::PSTableGroup::ShutDown();
  LOG(INFO) << "Rand Forest finished and shut down!";
  return 0;
}
