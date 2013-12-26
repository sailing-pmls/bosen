// Copyright (c) 2013, SailingLab
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

#ifndef PETUUM_INCLUDE_CONFIGS
#define PETUUM_INCLUDE_CONFIGS

#include <inttypes.h>
#include <stdint.h>

namespace petuum {

// For OpLogManager
struct OpLogManagerConfig {
  int thread_cache_capacity;
  int max_pending_op_logs;

  // num_servers and table_id are needed to serialize oplog into server
  // partitions.
  int num_servers;
  int table_id;
};

struct StorageConfig {

  // Size represents # of rows; cache eviction policy (including random
  // eviction) applies when exceeded.
  int capacity;

  // ===== LRURowStorage parameters =====
  //
  // These params will be used only if lru_params_are_defined == true. See
  // LRUStorage for the meanings of parameters.

  bool lru_params_are_defined;
  int active_list_size;
  double num_row_access_to_active;
};

// A class contains table configuration parameters. This is user-interfacing.
struct TableConfig {
  // ==== General Table configuration ====
  // TODO(wdai): use map of policies.
  //int32_t policy_id_;
  int32_t table_id;
  int32_t table_staleness;
  int32_t num_columns;

  // Number of threads on a client.
  int32_t num_threads;

  // Run time info needed to serialize oplog into server partitions.
  int num_servers;

  // Server storage config.
  StorageConfig server_storage_config;

  // Process cache configs.
  StorageConfig process_storage_config;

  // Thread cache + op log config.
  OpLogManagerConfig op_log_config;
};

template<typename V>
class ConsistencyPolicy;

template<typename V>
class ClientProxy;

// Constructor parameters for ConsistencyController.
template<typename V>
struct ConsistencyControllerConfig {

  // Each ConsistencyController is associated with a table.
  int32_t table_id;

  // Number of threads on a client.
  int32_t num_threads;

  // Run time info needed to serialize oplog into server partitions.
  int num_servers;

  StorageConfig storage_config;

  // ConsistencyController synchronizes based on policy.
  ConsistencyPolicy<V>* policy;

  // proxy is the only means ConsistencyController talks to servers.
  ClientProxy<V>* proxy;

  OpLogManagerConfig op_log_config;
};

}  // namespace petuum

#endif  // PETUUM_INCLUDE_CONFIGS
