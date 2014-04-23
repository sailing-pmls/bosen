#pragma once

#include <inttypes.h>
#include <stdint.h>
#include <map>
#include <vector>

#include "petuum_ps/include/host_info.hpp"

namespace petuum {

enum ConsistencyModel {
  // Stale synchronous parallel.
  SSP = 0,

  // SSP with server push
  // Assumes that all clients have the same number of bg threads.
  SSPPush = 1,

  SSPPushValueBound = 2
};

struct TableGroupConfig {

  TableGroupConfig():
      aggressive_clock(false),
      aggressive_cpu(false){ }

  // ================= Global Parameters ===================
  // Global parameters have to be the same across all processes.

  // Total number of servers in the system.
  int32_t num_total_server_threads;

  // Total number of tables the PS will have. Each init thread must make
  // num_tables CreateTable() calls.
  int32_t num_tables;

  // Total number of clients in the system.
  int32_t num_total_clients;

  // Number of total background worker threads in the system.
  int32_t num_total_bg_threads;

  // ===================== Local Parameters ===================
  // Local parameters can differ between processes, but have to sum up to global
  // parameters.

  // Number of local server threads.
  int32_t num_local_server_threads;

  // Number of local applications threads, including init thread.
  int32_t num_local_app_threads;

  // Number of local background worker threads.
  int32_t num_local_bg_threads;

  // IDs of all servers.
  std::vector<int32_t> server_ids;

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

};

// TableInfo is shared between client and server.
struct TableInfo {
  // table_staleness is used for SSP and ClockVAP.
  int32_t table_staleness;

  // A table can only have one type of row. The row_type is defined when
  // calling TableGroup::RegisterRow().
  int32_t row_type;

  // row_capacity can mean different thing for different row_type. For example
  // in vector-backed dense row it is the max number of columns. This
  // parameter is ignored for sparse row.
  int32_t row_capacity;
};

// ClientTableConfig is used by client only.
struct ClientTableConfig {
  TableInfo table_info;

  // In # of rows.
  int32_t process_cache_capacity;

  // In # of rows.
  int32_t thread_cache_capacity;

  // Estimated upper bound # of pending oplogs in terms of # of rows. For SSP
  // this is the # of rows all threads collectively touches in a Clock().
  int32_t oplog_capacity;
};

}  // namespace petuum
