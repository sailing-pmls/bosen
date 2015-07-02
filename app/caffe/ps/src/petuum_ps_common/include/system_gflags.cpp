#include <glog/logging.h>
#include <gflags/gflags.h>

#include <petuum_ps_common/include/configs.hpp>

DEFINE_string(stats_path, "", "stats file path prefix");

// Topology Configs
DEFINE_int32(num_clients, 1, "total number of clients");
DEFINE_int32(num_comm_channels_per_client, 1, "no. of comm channels per client");
DEFINE_bool(init_thread_access_table, false, "whether init thread accesses table");
DEFINE_int32(num_table_threads, 1, "no. of worker threads per client");
DEFINE_int32(client_id, 0, "This client's ID");
DEFINE_string(hostfile, "", "path to Petuum PS server configuration file");

// Execution Configs
DEFINE_string(consistency_model, "SSPPush", "SSPAggr/SSPPush/SSP");

// SSPAggr Configs -- client side
DEFINE_uint64(client_bandwidth_mbps, 40, "per-thread bandwidth limit, in mbps");
DEFINE_uint64(server_bandwidth_mbps, 40, "per-thread bandwidth limit, in mbps");
DEFINE_uint64(bg_idle_milli, 10, "Bg idle millisecond");

DEFINE_uint64(thread_oplog_batch_size, 100*1000*1000, "thread oplog batch size");

// SSPAggr Configs -- server side
DEFINE_uint64(row_candidate_factor, 5, "server row candidate factor");
DEFINE_int32(server_idle_milli, 10, "server idle time out in millisec");
DEFINE_string(update_sort_policy, "Random", "Update sort policy");

// Snapshot Configs
DEFINE_int32(snapshot_clock, -1, "snapshot clock");
DEFINE_int32(resume_clock, -1, "resume clock");
DEFINE_string(snapshot_dir, "", "snap shot directory");
DEFINE_string(resume_dir, "", "resume directory");

// numa flags
DEFINE_bool(numa_opt, false, "numa opt on?");
DEFINE_int32(numa_index, 0, "numa node index");
DEFINE_string(numa_policy, "Even", "numa policy");
DEFINE_bool(naive_table_oplog_meta, true, "naive table oplog meta");
DEFINE_bool(suppression_on, false, "suppression on");
DEFINE_bool(use_approx_sort, true, "use_approx_sort");

DEFINE_uint64(num_zmq_threads, 1, "number of zmq threads");
