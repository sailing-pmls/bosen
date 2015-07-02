#pragma once

#include <glog/logging.h>
#include <gflags/gflags.h>

#include <petuum_ps_common/include/configs.hpp>
#include <petuum_ps_common/util/utils.hpp>

DECLARE_string(stats_path);
// Topology Configs
DECLARE_int32(num_clients);
DECLARE_int32(num_comm_channels_per_client);
DECLARE_bool(init_thread_access_table);
DECLARE_int32(num_table_threads);
DECLARE_int32(client_id);
DECLARE_string(hostfile);

// Execution Configs
DECLARE_string(consistency_model);

// SSPAggr Configs -- client side
DECLARE_uint64(client_bandwidth_mbps);
DECLARE_uint64(server_bandwidth_mbps);
DECLARE_uint64(bg_idle_milli);

DECLARE_uint64(thread_oplog_batch_size);

// SSPAggr Configs -- server side
DECLARE_uint64(row_candidate_factor);
DECLARE_int32(server_idle_milli);
DECLARE_string(update_sort_policy);

// Snapshot Configs
DECLARE_int32(snapshot_clock);
DECLARE_int32(resume_clock);
DECLARE_string(snapshot_dir);
DECLARE_string(resume_dir);

// numa flags
DECLARE_bool(numa_opt);
DECLARE_int32(numa_index);
DECLARE_string(numa_policy);
DECLARE_bool(naive_table_oplog_meta);
DECLARE_bool(suppression_on);
DECLARE_bool(use_approx_sort);

DECLARE_uint64(num_zmq_threads);

namespace petuum {
void InitTableGroupConfig(TableGroupConfig *config, int32_t num_tables);

}
