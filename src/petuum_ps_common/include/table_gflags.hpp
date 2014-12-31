#pragma once

#include <petuum_ps_common/include/configs.hpp>

#include <glog/logging.h>
#include <gflags/gflags.h>

// just provide the definitions for simplicity

DEFINE_int32(table_staleness, 0, "table staleness");
DEFINE_int32(row_type, 0, "table row type");
DEFINE_int32(row_oplog_type, petuum::RowOpLogType::kDenseRowOpLog, "row oplog type");
DEFINE_bool(oplog_dense_serialized, true, "dense serialized oplog");

DEFINE_string(oplog_type, "Sparse", "use append only oplog?");
DEFINE_string(append_only_oplog_type, "Inc", "append only oplog type?");
DEFINE_uint64(append_only_buffer_capacity, 1024*1024, "buffer capacity in bytes");
DEFINE_uint64(append_only_buffer_pool_size, 3, "append_ only buffer pool size");
DEFINE_int32(bg_apply_append_oplog_freq, 4, "bg apply append oplog freq");
DEFINE_string(process_storage_type, "BoundedSparse", "proess storage type");

namespace petuum {

// user still need set thet following configuration parameters:
// 1) table_info.row_capacity
// 2) table_info.dense_row_oplog_capacity
// 3) proess_cache_capacity
// 4) thread_cache_capacity
// 5) oplog_capacity

void InitTableConfig(ClientTableConfig *config) {
  config->table_info.table_staleness = FLAGS_table_staleness;
  config->table_info.row_type = FLAGS_row_type;

  config->table_info.oplog_dense_serialized = FLAGS_oplog_dense_serialized;
  config->table_info.row_oplog_type = FLAGS_row_oplog_type;

  if (FLAGS_oplog_type == "Sparse") {
    config->oplog_type = Sparse;
  } else if (FLAGS_oplog_type == "AppendOnly"){
    config->oplog_type = AppendOnly;
    if (FLAGS_append_only_oplog_type == "Inc") {
      config->append_only_oplog_type = Inc;
    } else if (FLAGS_append_only_oplog_type == "BatchInc") {
      config->append_only_oplog_type = BatchInc;
    } else if (FLAGS_append_only_oplog_type == "DenseBatchInc") {
      config->append_only_oplog_type = DenseBatchInc;
    } else {
      LOG(FATAL) << "Unknown append only oplog type = " << FLAGS_append_only_oplog_type;
    }
  } else if (FLAGS_oplog_type == "Dense") {
    config->oplog_type = Dense;
  } else {
    LOG(FATAL) << "Unknown oplog type = " << FLAGS_oplog_type;
  }

  config->append_only_buff_capacity = FLAGS_append_only_buffer_capacity;
  config->per_thread_append_only_buff_pool_size = FLAGS_append_only_buffer_pool_size;
  config->bg_apply_append_oplog_freq = FLAGS_bg_apply_append_oplog_freq;

  if (FLAGS_process_storage_type == "BoundedDense") {
    config->process_storage_type = petuum::BoundedDense;
  } else if (FLAGS_process_storage_type == "BoundedSparse") {
    config->process_storage_type = petuum::BoundedSparse;
  } else {
    LOG(FATAL) << "Unknown process storage type " << FLAGS_process_storage_type;
  }
}

}
