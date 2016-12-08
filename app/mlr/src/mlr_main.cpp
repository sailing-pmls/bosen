// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.10.04

#include "mlr_engine.hpp"
#include "common.hpp"
#include <petuum_ps_common/include/petuum_ps.hpp>
#include <petuum_ps_common/include/system_gflags.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <thread>
#include <vector>
#include <cstdint>
#include <algorithm>

// Petuum Parameters
//DEFINE_string(hostfile, "", "Path to file containing server ip:port.");
//DEFINE_int32(num_clients, 1, "Total number of clients");
//DEFINE_int32(num_app_threads, 1, "Number of app threads in this client");
//DEFINE_int32(client_id, 0, "Client ID");
//DEFINE_string(consistency_model, "SSPPush", "SSP or SSPPush");
//DEFINE_string(stats_path, "", "Statistics output file");
//DEFINE_int32(num_comm_channels_per_client, 1,
//   "number of comm channels per client");

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
DEFINE_int32(num_train_eval, 20, "Use the next num_train_eval train data "
    "(per thread) for intermediate eval.");
DEFINE_int32(num_test_eval, 20, "Use the first num_test_eval test data for "
    "intermediate eval. 0 for using all. The final eval will always use all "
    "test data.");
DEFINE_bool(perform_test, false, "Ignore test_file if true.");
DEFINE_bool(use_weight_file, false, "True to use init_weight_file as init");
DEFINE_string(weight_file, "", "Use this file to initialize weight. "
  "Format of the file is libsvm (see SaveWeight in MLRSGDSolver).");

// MLR Parameters
DEFINE_int32(num_epochs, 1, "Number of data sweeps.");
DEFINE_int32(num_batches_per_epoch, 10, "Since we Clock() at the end of each batch, "
    "num_batches_per_epoch is effectively the number of clocks per epoch (iteration)");
DEFINE_double(init_lr, 0.1, "Initial learning rate");
DEFINE_double(lr_decay_rate, 1, "multiplicative decay");
DEFINE_int32(num_batches_per_eval, 10, "Number of batches per evaluation");
DEFINE_double(lambda, 0, "L2 regularization parameter.");

// Misc
DEFINE_string(output_file_prefix, "", "Results go here.");
DEFINE_int32(w_table_id, 0, "Weight table's ID in PS.");
DEFINE_int32(loss_table_id, 1, "Loss table's ID in PS.");
DEFINE_int32(staleness, 0, "staleness for weight tables.");
DEFINE_int32(row_oplog_type, petuum::RowOpLogType::kDenseRowOpLog,
    "row oplog type");
DEFINE_bool(oplog_dense_serialized, false, "True to not squeeze out the 0's "
    "in dense oplog.");
DEFINE_int32(num_secs_per_checkpoint, 600, "# of seconds between each saving "
    "to disk");
DEFINE_int32(w_table_num_cols, 1000000,
    "# of columns in w_table. Only used for binary LR.");

const int32_t kDenseRowFloatTypeID = 0;

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  //STATS_APP_LOAD_DATA_BEGIN();
  mlr::MLREngine mlr_engine;
  mlr_engine.ReadData();
  //STATS_APP_LOAD_DATA_END();

  int32_t feature_dim = mlr_engine.GetFeatureDim();
  int32_t num_labels = mlr_engine.GetNumLabels();

  petuum::TableGroupConfig table_group_config;
  table_group_config.num_comm_channels_per_client
    = FLAGS_num_comm_channels_per_client;
  table_group_config.num_total_clients = FLAGS_num_clients;
  // a weight table and a loss table.
  table_group_config.num_tables = 2;
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

  petuum::PSTableGroup::RegisterRow<petuum::DenseRow<float> >
    (kDenseRowFloatTypeID);

  // Use false to disallow main thread to access table API.
  petuum::PSTableGroup::Init(table_group_config, false);

  // Creating weight table: one row per label.
  petuum::ClientTableConfig table_config;
  table_config.table_info.row_type = kDenseRowFloatTypeID;

  table_config.table_info.table_staleness = FLAGS_staleness;
  table_config.table_info.row_capacity =
    std::min(FLAGS_w_table_num_cols, feature_dim);
  table_config.table_info.dense_row_oplog_capacity =
    table_config.table_info.row_capacity;
  table_config.table_info.row_oplog_type = FLAGS_row_oplog_type;
  table_config.table_info.oplog_dense_serialized =
    FLAGS_oplog_dense_serialized;
  table_config.process_cache_capacity =
    std::ceil(static_cast<float>(feature_dim) /
        table_config.table_info.row_capacity) * num_labels;
  table_config.oplog_capacity = table_config.process_cache_capacity;
  petuum::PSTableGroup::CreateTable(FLAGS_w_table_id, table_config);

  // loss table.
  table_config.table_info.row_type = kDenseRowFloatTypeID;
  table_config.table_info.table_staleness = 0;
  table_config.table_info.row_capacity = mlr::kNumColumnsLossTable;
  table_config.process_cache_capacity = 1000;
  table_config.oplog_capacity = table_config.process_cache_capacity;
  petuum::PSTableGroup::CreateTable(FLAGS_loss_table_id, table_config);

  petuum::PSTableGroup::CreateTableDone();

  LOG(INFO) << "Starting MLR with " << FLAGS_num_app_threads << " threads "
    << "on client " << FLAGS_client_id;

  std::vector<std::thread> threads(FLAGS_num_app_threads);
  for (auto& thr : threads) {
    thr = std::thread(&mlr::MLREngine::Start, std::ref(mlr_engine));
  }
  for (auto& thr : threads) {
    thr.join();
  }

  petuum::PSTableGroup::ShutDown();
  LOG(INFO) << "MLR finished and shut down!";
  return 0;
}
