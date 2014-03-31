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

#include <glog/logging.h>
#include <map>
#include <vector>

#include <petuum_ps/include/configs.hpp>
#include <petuum_ps/include/table.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps/thread/bg_workers.hpp>
#include <petuum_ps/comm_bus/comm_bus.hpp>
#include <petuum_ps/server/server_threads.hpp>
#include <petuum_ps/include/host_info.hpp>

int main(int argc, char *argv[]) {
  google::InitGoogleLogging(argv[0]);

  int32_t num_servers = 2;
  int32_t name_node_id = 0;
  int32_t num_local_server_threads = 2;
  int32_t num_app_threads = 1;
  int32_t num_table_threads = 1;
  int32_t num_bg_threads = 1;
  int32_t num_total_bg_threads = 1;
  int32_t num_tables = 1;
  int32_t num_clients = 1;
  int32_t client_id = 0;
  int32_t app_thread_id = 3;
  int32_t bg_thread_id = 2;
  int32_t server_ring_size = 0;
  petuum::ConsistencyModel consistency_model = petuum::SSP;
  std::vector<int32_t> server_ids(2);
  server_ids[0] = 0;
  server_ids[1] = 1;
  std::map<int32_t, petuum::HostInfo> host_map;

  int32_t entity_id_st = 0, entity_id_end = 3;
  petuum::CommBus *comm_bus = new petuum::CommBus(entity_id_st, entity_id_end, 
    1);

  petuum::GlobalContext::Init(num_servers,
    name_node_id,
    num_local_server_threads,
    num_app_threads,
    num_table_threads,
    num_bg_threads,
    num_total_bg_threads,
    num_tables,
    num_clients,
    server_ids,
    host_map,
    client_id,
    server_ring_size,
    consistency_model);
  petuum::GlobalContext::comm_bus = comm_bus;
  LOG(INFO) << "Initialized global context";

  petuum::CommBus::Config comm_config(app_thread_id, petuum::CommBus::kNone,
    "");
  comm_bus->ThreadRegister(comm_config);

  std::map<int32_t, petuum::SystemTable* > tables;

  petuum::ServerThreads::Init(0);

  petuum::BgWorkers::Init(bg_thread_id, &tables);

  LOG(INFO) << "App thread BgWorkers Init Done";

  petuum::ClientTableConfig table_config;

  petuum::BgWorkers::CreateTable(0, table_config);
  petuum::BgWorkers::WaitCreateTable();

  petuum::BgWorkers::RequestRow(0, 1, 0);

  LOG(INFO) << "App thread has registered";

  petuum::BgWorkers::ThreadDeregister();

  petuum::ServerThreads::ShutDown();

  petuum::BgWorkers::ShutDown();

  comm_bus->ThreadDeregister();

  return 0;
}
