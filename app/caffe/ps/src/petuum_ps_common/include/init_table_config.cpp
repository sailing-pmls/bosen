#include <petuum_ps_common/include/table_gflags_declare.hpp>
#include <petuum_ps_common/include/configs.hpp>
#include <petuum_ps_common/util/utils.hpp>

namespace petuum {
// user still need set thet following configuration parameters:
// 1) table_info.row_capacity
// 2) table_info.dense_row_oplog_capacity
// 3) proess_cache_capacity
// 4) thread_cache_capacity
// 5) oplog_capacity

void InitTableConfig(ClientTableConfig *config) {
  CHECK(std::is_pod<ClientTableConfig>::value);
  config->table_info.table_staleness = FLAGS_table_staleness;
  config->table_info.row_type = FLAGS_row_type;

  config->table_info.oplog_dense_serialized = FLAGS_oplog_dense_serialized;
  config->table_info.row_oplog_type = FLAGS_row_oplog_type;

  config->oplog_type = GetOpLogType(FLAGS_oplog_type);

  if (config->oplog_type == AppendOnly) {
    config->append_only_oplog_type
        = GetAppendOnlyOpLogType(FLAGS_append_only_oplog_type);
  }

  config->append_only_buff_capacity = FLAGS_append_only_buffer_capacity;
  config->per_thread_append_only_buff_pool_size
      = FLAGS_append_only_buffer_pool_size;
  config->bg_apply_append_oplog_freq = FLAGS_bg_apply_append_oplog_freq;

  config->process_storage_type
      = GetProcessStroageType(FLAGS_process_storage_type);

  config->no_oplog_replay = FLAGS_no_oplog_replay;
  config->table_info.server_push_row_upper_bound
      = FLAGS_server_push_row_upper_bound;
  config->client_send_oplog_upper_bound
      = FLAGS_client_send_oplog_upper_bound;
  config->table_info.server_table_logic = FLAGS_server_table_logic;
  config->table_info.version_maintain = FLAGS_version_maintain;
}

}
