// Copyright (c) 2014, Sailing Lab
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the <ORGANIZATION> nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
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

  // Value-bound (between pair-wise processes) Asynchronous parallel.
  VAP = 1,

  // Value-bound + SSP.
  // TODO(wdai): Week or strong VAP?
  ClockVAP = 2
};

struct TableGroupConfig {

  TableGroupConfig():
  aggressive_clock(false) { }

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

  // Each thread within the process (including app, server, and bg threads) is
  // assigned a globally unique ID. It is required that the IDs of threads in
  // the same process are in a contiguous range [local_id_min, local_id_max],
  // inclusive.
  int32_t local_id_min;
  int32_t local_id_max;

  // If set to true, oplog send is triggered on every Clock() call.
  // If set to false, oplog is only sent if the process clock (representing all
  // app threads) has advanced.
  // Aggressive clock may reduce memory footprint and improve the per-clock
  // convergence rate in the cost of performance.
  // Default is false (suggested).
  bool aggressive_clock;

  ConsistencyModel consistency_model;

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

  // TODO(wdai)
  int32_t oplog_capacity;
};

}  // namespace petuum
