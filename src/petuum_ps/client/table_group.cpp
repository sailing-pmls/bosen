#include <petuum_ps_common/util/stats.hpp>
#include <petuum_ps/client/table_group.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps/server/server_threads.hpp>
#include <petuum_ps/server/name_node.hpp>
#include <petuum_ps/thread/bg_workers.hpp>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <petuum_ps/thread/numa_mgr.hpp>

namespace petuum {
TableGroup::TableGroup(const TableGroupConfig &table_group_config,
                       bool table_access, int32_t *init_thread_id):
    AbstractTableGroup(),
    max_table_staleness_(0) {

  int32_t num_comm_channels_per_client
      = table_group_config.num_comm_channels_per_client;
  int32_t num_local_app_threads = table_group_config.num_local_app_threads;
  int32_t num_local_table_threads = table_access ? num_local_app_threads
    : (num_local_app_threads - 1);

  int32_t num_tables = table_group_config.num_tables;
  int32_t num_total_clients = table_group_config.num_total_clients;
  const std::map<int32_t, HostInfo> &host_map = table_group_config.host_map;

  int32_t client_id = table_group_config.client_id;
  int32_t server_ring_size = table_group_config.server_ring_size;
  ConsistencyModel consistency_model = table_group_config.consistency_model;
  int32_t local_id_min = GlobalContext::get_thread_id_min(client_id);
  int32_t local_id_max = GlobalContext::get_thread_id_max(client_id);
  num_app_threads_registered_ = 1;  // init thread is the first one

  STATS_INIT(table_group_config);
  STATS_REGISTER_THREAD(kAppThread);

  // can be Inited after CommBus but must be before everything else
  GlobalContext::Init(
      num_comm_channels_per_client,
      num_local_app_threads,
      num_local_table_threads,
      num_tables,
      num_total_clients,
      host_map,
      client_id,
      server_ring_size,
      consistency_model,
      table_group_config.aggressive_cpu,
      table_group_config.snapshot_clock,
      table_group_config.snapshot_dir,
      table_group_config.resume_clock,
      table_group_config.resume_dir,
      table_group_config.update_sort_policy,
      table_group_config.bg_idle_milli,
      table_group_config.bandwidth_mbps,
      table_group_config.oplog_push_upper_bound_kb,
      table_group_config.oplog_push_staleness_tolerance,
      table_group_config.thread_oplog_batch_size,
      table_group_config.server_push_row_threshold,
      table_group_config.server_idle_milli,
      table_group_config.server_row_candidate_factor);

  NumaMgr::Init(table_group_config.numa_opt);

  CommBus *comm_bus = new CommBus(local_id_min, local_id_max,
                                  num_total_clients, 1);
  GlobalContext::comm_bus = comm_bus;

  *init_thread_id = local_id_min
                    + GlobalContext::kInitThreadIDOffset;
  CommBus::Config comm_config(*init_thread_id, CommBus::kNone, "");

  GlobalContext::comm_bus->ThreadRegister(comm_config);
  ThreadContext::RegisterThread(*init_thread_id);

  if (GlobalContext::am_i_name_node_client()) {
    NameNode::Init();
    ServerThreads::Init(local_id_min + 1);
  } else {
    ServerThreads::Init(local_id_min);
  }

  BgWorkers::Start(&tables_);
  BgWorkers::AppThreadRegister();

  if (table_access) {
    vector_clock_.AddClock(*init_thread_id, 0);
    NumaMgr::ConfigureTableThread();
  }

  if (table_group_config.aggressive_clock)
    ClockInternal = &TableGroup::ClockAggressive;
  else
    ClockInternal = &TableGroup::ClockConservative;
}

TableGroup::~TableGroup() {
  pthread_barrier_destroy(&register_barrier_);
  BgWorkers::AppThreadDeregister();
  ServerThreads::ShutDown();

  if (GlobalContext::am_i_name_node_client())
    NameNode::ShutDown();

  BgWorkers::ShutDown();
  GlobalContext::comm_bus->ThreadDeregister();

  delete GlobalContext::comm_bus;
  for(auto iter = tables_.begin(); iter != tables_.end(); iter++){
    delete iter->second;
  }
  STATS_DEREGISTER_THREAD();
  STATS_PRINT();
}

bool TableGroup::CreateTable(int32_t table_id,
  const ClientTableConfig& table_config) {
  max_table_staleness_ = std::max(max_table_staleness_,
      table_config.table_info.table_staleness);

  bool suc = BgWorkers::CreateTable(table_id, table_config);
  if (suc
      && (GlobalContext::get_num_app_threads()
	  == GlobalContext::get_num_table_threads())) {
    auto iter = tables_.find(table_id);
    iter->second->RegisterThread();
  }
  return suc;
}

void TableGroup::CreateTableDone() {
  BgWorkers::WaitCreateTable();
  pthread_barrier_init(&register_barrier_, 0,
    GlobalContext::get_num_table_threads());
}

void TableGroup::WaitThreadRegister() {
  if (GlobalContext::get_num_table_threads() ==
      GlobalContext::get_num_app_threads()) {
    pthread_barrier_wait(&register_barrier_);
  }
}

int32_t TableGroup::RegisterThread() {
  STATS_REGISTER_THREAD(kAppThread);
  int app_thread_id_offset = num_app_threads_registered_++;

  int32_t thread_id = GlobalContext::get_local_id_min()
    + GlobalContext::kInitThreadIDOffset + app_thread_id_offset;

  petuum::CommBus::Config comm_config(thread_id, petuum::CommBus::kNone, "");

  ThreadContext::RegisterThread(thread_id);

  NumaMgr::ConfigureTableThread();

  GlobalContext::comm_bus->ThreadRegister(comm_config);

  BgWorkers::AppThreadRegister();

  vector_clock_.AddClock(thread_id, 0);

  for (auto table_iter = tables_.cbegin(); table_iter != tables_.cend();
       table_iter++) {
    table_iter->second->RegisterThread();
  }

  pthread_barrier_wait(&register_barrier_);
  return thread_id;
}

void TableGroup::DeregisterThread(){
  for (auto table_iter = tables_.cbegin(); table_iter != tables_.cend();
       table_iter++) {
    table_iter->second->DeregisterThread();
  }

  BgWorkers::AppThreadDeregister();
  GlobalContext::comm_bus->ThreadDeregister();
  STATS_DEREGISTER_THREAD();
}

void TableGroup::Clock() {
  STATS_APP_ACCUM_TG_CLOCK_BEGIN();
  ThreadContext::Clock();
  (this->*ClockInternal)();
  STATS_APP_ACCUM_TG_CLOCK_END();
}

void TableGroup::GlobalBarrier() {
  for (int i = 0; i < max_table_staleness_ + 1; ++i) {
    Clock();
  }
}

void TableGroup::ClockAggressive() {
  for (auto table_iter = tables_.cbegin(); table_iter != tables_.cend();
    table_iter++) {
    table_iter->second->Clock();
  }
  int clock = vector_clock_.Tick(ThreadContext::get_id());
  if (clock != 0) {
    BgWorkers::ClockAllTables();
  } else {
    BgWorkers::SendOpLogsAllTables();
  }
}

void TableGroup::ClockConservative() {
  for (auto table_iter = tables_.cbegin(); table_iter != tables_.cend();
    table_iter++) {
    table_iter->second->Clock();
  }
  int clock = vector_clock_.Tick(ThreadContext::get_id());
  if (clock != 0) {
    BgWorkers::ClockAllTables();
  }
}
}
