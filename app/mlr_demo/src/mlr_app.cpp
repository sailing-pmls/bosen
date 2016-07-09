// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.10.04
// Modified by: Binghong Chen (2016.06.30)

#include <petuum_ps_common/include/ps_application.hpp>
#include "mlr_engine.hpp"
#include "common.hpp"
#include <petuum_ps_common/include/petuum_ps.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <thread>
#include <vector>
#include <cstdint>
#include <algorithm>



class MLRApp : public PsApplication {
public:
  const int32_t kDenseRowFloatTypeID = 0;
  mlr::MLREngine mlr_engine;
  
  void initialize(petuum::TableGroupConfig &table_group_config) {
    //STATS_APP_LOAD_DATA_BEGIN();
    mlr_engine.ReadData();
    //STATS_APP_LOAD_DATA_END();

    int32_t feature_dim = mlr_engine.GetFeatureDim();
    int32_t num_labels = mlr_engine.GetNumLabels();

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
  }
  
  void runWorkerThread(int threadId) {
    mlr_engine.Start();
  }
};
