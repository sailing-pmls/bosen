#include <petuum_ps_common/include/system_gflags_declare.hpp>
#include <petuum_ps_common/include/configs.hpp>
#include <petuum_ps_common/util/utils.hpp>

namespace petuum {
void InitTableGroupConfig(TableGroupConfig *config, int32_t num_tables) {
  config->stats_path = FLAGS_stats_path;
  config->num_comm_channels_per_client = FLAGS_num_comm_channels_per_client;
  config->num_tables = num_tables;
  config->num_total_clients = FLAGS_num_clients;
  config->num_local_app_threads = FLAGS_init_thread_access_table ?
                                  FLAGS_num_table_threads : FLAGS_num_table_threads + 1;

  GetHostInfos(FLAGS_hostfile, &(config->host_map));

  config->client_id = FLAGS_client_id;

  config->consistency_model = GetConsistencyModel(FLAGS_consistency_model);

  config->aggressive_clock = false;
  config->aggressive_cpu = false;
  config->server_ring_size = 0;

  config->snapshot_clock = FLAGS_snapshot_clock;
  config->resume_clock = FLAGS_resume_clock;
  config->snapshot_dir = FLAGS_snapshot_dir;
  config->resume_dir = FLAGS_resume_dir;

  config->update_sort_policy = GetUpdateSortPolicy(FLAGS_update_sort_policy);

  config->bg_idle_milli = FLAGS_bg_idle_milli;
  config->client_bandwidth_mbps = FLAGS_client_bandwidth_mbps;
  config->server_bandwidth_mbps = FLAGS_server_bandwidth_mbps;
  config->thread_oplog_batch_size = FLAGS_thread_oplog_batch_size;
  config->row_candidate_factor = FLAGS_row_candidate_factor;
  config->server_idle_milli = FLAGS_server_idle_milli;

  config->numa_opt = FLAGS_numa_opt;
  config->numa_index = FLAGS_numa_index;

  if (FLAGS_numa_opt) {
    if (FLAGS_numa_policy == "Even") {
      config->numa_policy = Even;
    } else if (FLAGS_numa_policy == "Center") {
      config->numa_policy = Center;
    } else {
      LOG(FATAL) << "Unknown NUMA policy = " << FLAGS_numa_policy;
    }
  }

  config->naive_table_oplog_meta = FLAGS_naive_table_oplog_meta;
  config->suppression_on = FLAGS_suppression_on;
  config->use_approx_sort = FLAGS_use_approx_sort;

  config->num_zmq_threads = FLAGS_num_zmq_threads;
}

}
