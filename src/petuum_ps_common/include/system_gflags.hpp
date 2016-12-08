#pragma once

#include <glog/logging.h>
#include <gflags/gflags.h>

#include <petuum_ps_common/include/configs.hpp>
#include <petuum_ps_common/util/utils.hpp>
#include <cstdint>

DECLARE_string(stats_path);

DECLARE_int32(num_clients);
DECLARE_string(hostfile);
DECLARE_int32(num_comm_channels_per_client);
DECLARE_bool(init_thread_access_table);
DECLARE_int32(num_table_threads);
DECLARE_int32(client_id);
DECLARE_int32(num_app_threads);

DECLARE_string(consistency_model);
DECLARE_uint64(bandwidth_mbps);
DECLARE_uint64(bg_idle_milli);
DECLARE_uint64(oplog_push_upper_bound_kb);
DECLARE_int32(oplog_push_staleness_tolerance);
DECLARE_uint64(thread_oplog_batch_size);

DECLARE_uint64(server_row_candidate_factor);
DECLARE_int32(server_push_row_threshold);
DECLARE_int32(server_idle_milli);
DECLARE_string(update_sort_policy);

DECLARE_int32(snapshot_clock);
DECLARE_int32(resume_clock);
DECLARE_string(snapshot_dir);
DECLARE_string(resume_dir);

namespace petuum {

void InitTableGroupConfig(TableGroupConfig *config, int32_t *client_id,
                          int32_t num_tables);
}
