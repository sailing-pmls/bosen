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


#include "petuum_ps/util/utils.hpp"
#include "petuum_ps/include/configs.hpp"
#include <fstream>
#include <iostream>
#include <glog/logging.h>

namespace petuum {

std::vector<petuum::ServerInfo> GetServerInfos(std::string server_file) {
  std::vector<petuum::ServerInfo> servers;
  std::ifstream input(server_file.c_str());
  std::string line;
  while(std::getline(input, line)){

    size_t pos = line.find_first_of("\t ");
    std::string idstr = line.substr(0, pos);

    size_t pos_ip = line.find_first_of("\t ", pos + 1);
    std::string ip = line.substr(pos + 1, pos_ip - pos - 1);
    std::string port = line.substr(pos_ip + 1);

    int32_t id =  atoi(idstr.c_str());
    servers.push_back(petuum::ServerInfo(id, ip, port));
    VLOG(0) << "get server: " << id << ":" << ip << ":" << port;
  }
  input.close();
  return servers;
}


std::map<int, TableConfig> ReadTableConfigs(std::string config_file) {
  libconfig::Config cfg;
  try {
    cfg.readFile(config_file.c_str());
  }
  catch(const libconfig::FileIOException &fioex)
  {
    LOG(FATAL) << "I/O error while reading file " << config_file;
  }
  catch(const libconfig::ParseException &pex)
  {
    LOG(FATAL) << "Parse error at " << pex.getFile() << ":" << pex.getLine()
      << " - " << pex.getError();
  }

  const libconfig::Setting &root = cfg.getRoot();
  if (!root.exists("table_configs")) {
    LOG(FATAL) << "table_configs is a required item in config file "
      << config_file;
  }

  // Get table_configs
  const libconfig::Setting &table_configs = root["table_configs"];
  int num_tables = table_configs.getLength();
  VLOG(0) << "Found " << num_tables << " table configs in config file: "
    << config_file;

  // Allocate TableConfig vector
  std::map<int, TableConfig> table_configs_map;

  // Process each table config in table_configs.
  for (int i = 0; i < num_tables; ++i) {
    const libconfig::Setting& table_config = table_configs[i];
    // Get table ID
    int32_t table_id;
    if (!table_config.lookupValue("table_id", table_id)) {
      LOG(FATAL) << "table_id is a required field in table_configs.";
    }

    // Get num columns
    int32_t num_columns;
    if (!table_config.lookupValue("num_columns", num_columns)) {
      LOG(FATAL) << "In table " << table_id
        << ": num_columns is a required field in table_configs.";
    }

    // Get table_staleness
    int32_t table_staleness;
    if (!table_config.lookupValue("table_staleness", table_staleness)) {
      LOG(FATAL) << "In table " << table_id
        << ": table_staleness is a required field in table_configs.";
    }

    // Get process storage config.
    if (!table_config.exists("process_storage_config")) {
      LOG(FATAL) << "In table " << table_id
        << ": process_storage_config is a required field in table_configs.";
    }
    // Get process storage max size.
    const libconfig::Setting& process_storage_cfg =
      table_config["process_storage_config"];
    int32_t process_storage_capacity;
    if (!process_storage_cfg.lookupValue("capacity",
          process_storage_capacity)) {
      LOG(FATAL) << "In table " << table_id
        << ": capacity is a required field in process_storage_config.";
    }

    // Get server storage max size.
    const libconfig::Setting& server_storage_cfg =
      table_config["server_storage_config"];
    int32_t server_storage_capacity;
    if (!server_storage_cfg.lookupValue("capacity", server_storage_capacity)) {
      LOG(FATAL) << "In table " << table_id
        << ": capacity is a required field in server_storage_config.";
    }

    // Get thread level config.
    if (!table_config.exists("thread_level_config")) {
      LOG(FATAL) << "In table " << table_id
        << ": thread_level_config is a required field in table_configs.";
    }
    const libconfig::Setting& thread_level_config =
      table_config["thread_level_config"];

    // Get thread_cache_capacity.
    int32_t thread_cache_capacity;
    if (!thread_level_config.lookupValue("thread_cache_capacity",
          thread_cache_capacity)) {
      LOG(FATAL) << "In table " << table_id
        << ": thread_cache_capacity is a required field in "
        << "thread_level_config.";
    }

    // Get thread_cache_capacity.
    int32_t max_pending_op_logs;
    int32_t default_max_pending_op_logs = 10;   // TODO(wdai): use default config.
    if (!thread_level_config.lookupValue("max_pending_op_logs",
          max_pending_op_logs)) {
      LOG(WARNING) << "In table " << table_id
        << ": max_pending_op_logs is undefined. Use default value: "
        << default_max_pending_op_logs;
    }

    // Create component config in TableConfig:
    // StorageConfig for process cache
    petuum::StorageConfig process_storage_config;
    process_storage_config.capacity = process_storage_capacity;

    // StorageConfig for server storage
    // TODO(wdai): remove server side config.
    petuum::StorageConfig server_storage_config;
    server_storage_config.capacity = server_storage_capacity;

    // OpLogManagerConfig
    petuum::OpLogManagerConfig op_log_config;
    op_log_config.thread_cache_capacity = thread_cache_capacity;
    op_log_config.max_pending_op_logs = max_pending_op_logs;

    // Create TableConfig.
    petuum::TableConfig tconfig;
    tconfig.table_staleness = table_staleness;
    tconfig.num_columns = num_columns;
    tconfig.server_storage_config = server_storage_config;
    tconfig.process_storage_config = process_storage_config;
    tconfig.op_log_config = op_log_config;

    table_configs_map[table_id] = tconfig;
  }
  return table_configs_map;
}

}   // namespace petuum
