#pragma once

#include <inttypes.h>
#include <stdint.h>
#include <map>
#include <vector>
#include <string>

#include <petuum_ps_common/include/host_info.hpp>
#include <petuum_ps_common/include/constants.hpp>

namespace petuum {

enum ConsistencyModel {
  // Stale synchronous parallel.
  SSP = 0,

  // SSP with server push
  // Assumes that all clients have the same number of bg threads.
  SSPPush = 1,

  SSPAggr = 2,

  LocalOOC = 6
};

enum UpdateSortPolicy {
  FIFO = 0,
  Random = 1,
  RelativeMagnitude = 2,
  FIFO_N_ReMag = 3
};

struct RowOpLogType {
  static const int32_t kDenseRowOpLog = 0;
  static const int32_t kSparseRowOpLog = 1;
  static const int32_t kSparseVectorRowOpLog = 2;
};

enum OpLogType {
  Sparse = 0,
  AppendOnly = 1,
  Dense = 2
};

enum AppendOnlyOpLogType {
  Inc = 0,
  BatchInc = 1,
  DenseBatchInc = 2
};

enum ProcessStorageType {
  BoundedDense = 0,
  BoundedSparse = 1
};

struct TableGroupConfig {

  TableGroupConfig():
      stats_path(""),
      num_comm_channels_per_client(1),
      num_tables(1),
      num_total_clients(1),
      num_local_app_threads(2),
      aggressive_clock(false),
      aggressive_cpu(false),
      snapshot_clock(-1),
      resume_clock(-1),
      update_sort_policy(Random),
      bg_idle_milli(2),
      bandwidth_mbps(40),
      oplog_push_upper_bound_kb(100),
      oplog_push_staleness_tolerance(2),
      thread_oplog_batch_size(100*1000*1000),
      server_row_candidate_factor(5),
      numa_opt(false) { }

  std::string stats_path;

  // ================= Global Parameters ===================
  // Global parameters have to be the same across all processes.

  // Total number of servers in the system.
  int32_t num_comm_channels_per_client;

  // Total number of tables the PS will have. Each init thread must make
  // num_tables CreateTable() calls.
  int32_t num_tables;

  // Total number of clients in the system.
  int32_t num_total_clients;

  // ===================== Local Parameters ===================
  // Local parameters can differ between processes, but have to sum up to global
  // parameters.

  // Number of local applications threads, including init thread.
  int32_t num_local_app_threads;

  // mapping server ID to host info.
  std::map<int32_t, HostInfo> host_map;

  // My client id.
  int32_t client_id;

  // If set to true, oplog send is triggered on every Clock() call.
  // If set to false, oplog is only sent if the process clock (representing all
  // app threads) has advanced.
  // Aggressive clock may reduce memory footprint and improve the per-clock
  // convergence rate in the cost of performance.
  // Default is false (suggested).
  bool aggressive_clock;

  ConsistencyModel consistency_model;
  int32_t aggressive_cpu;

  // In Async+pushing,
  int32_t server_ring_size;

  int32_t snapshot_clock;
  int32_t resume_clock;
  std::string snapshot_dir;
  std::string resume_dir;

  std::string ooc_path_prefix;

  UpdateSortPolicy update_sort_policy;

  // In number of milliseconds.
  // If the bg thread wakes up and finds there's no work to do,
  // it goes back to sleep for this much time or until it receives
  // a message.
  long bg_idle_milli;

  // Bandwidth in Megabits per second
  double bandwidth_mbps;

  // upper bound on update message size in kilobytes
  size_t oplog_push_upper_bound_kb;

  int32_t oplog_push_staleness_tolerance;

  size_t thread_oplog_batch_size;

  size_t server_push_row_threshold;

  long server_idle_milli;

  long server_row_candidate_factor;

  bool numa_opt;
};

// TableInfo is shared between client and server.
struct TableInfo {
  TableInfo():
      table_staleness(0),
      row_type(-1),
      row_capacity(0),
      oplog_dense_serialized(false),
      row_oplog_type(1),
      dense_row_oplog_capacity(0) { }

  // table_staleness is used for SSP and ClockVAP.
  int32_t table_staleness;

  // A table can only have one type of row. The row_type is defined when
  // calling TableGroup::RegisterRow().
  int32_t row_type;

  // row_capacity can mean different thing for different row_type. For example
  // in vector-backed dense row it is the max number of columns. This
  // parameter is ignored for sparse row.
  size_t row_capacity;

  bool oplog_dense_serialized;

  int32_t row_oplog_type;

  size_t dense_row_oplog_capacity;
};

// ClientTableConfig is used by client only.
struct ClientTableConfig {
  ClientTableConfig():
      process_cache_capacity(0),
      thread_cache_capacity(1),
      oplog_capacity(0),
      oplog_type(Dense),
      append_only_oplog_type(Inc),
      append_only_buff_capacity(10*k1_Mi),
      per_thread_append_only_buff_pool_size(3),
      bg_apply_append_oplog_freq(1),
      process_storage_type(BoundedSparse),
      no_oplog_replay(false) { }

  TableInfo table_info;

  // In # of rows.
  size_t process_cache_capacity;

  // In # of rows.
  size_t thread_cache_capacity;

  // Estimated upper bound # of pending oplogs in terms of # of rows. For SSP
  // this is the # of rows all threads collectively touches in a Clock().
  size_t oplog_capacity;

  OpLogType oplog_type;

  AppendOnlyOpLogType append_only_oplog_type;

  size_t append_only_buff_capacity;

  size_t per_thread_append_only_buff_pool_size; // per partition

  int32_t bg_apply_append_oplog_freq;

  ProcessStorageType process_storage_type;

  bool no_oplog_replay;
};

}  // namespace petuum
