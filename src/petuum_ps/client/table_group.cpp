#include "petuum_ps/include/table_group.hpp"
#include "petuum_ps/thread/context.hpp"
#include "petuum_ps/util/stats.hpp"
#include "petuum_ps/server/server_threads.hpp"
#include "petuum_ps/server/name_node_thread.hpp"
#include "petuum_ps/thread/bg_workers.hpp"
#include <iostream>
#include <algorithm>

namespace petuum {
std::map<int32_t, ClientTable*> TableGroup::tables_;
pthread_barrier_t TableGroup::register_barrier_;
std::atomic<int> TableGroup::num_app_threads_registered_;
TableGroup::ClockFunc TableGroup::ClockInternal;
int32_t TableGroup::max_table_staleness_ = 0;
VectorClockMT TableGroup::vector_clock_;

int32_t TableGroup::Init(const TableGroupConfig &table_group_config,
  bool table_access) {

  int32_t num_total_server_threads
    = table_group_config.num_total_server_threads;
  int32_t num_local_server_threads
    = table_group_config.num_local_server_threads;
  int32_t num_local_app_threads = table_group_config.num_local_app_threads;
  int32_t num_local_table_threads = table_access ? num_local_app_threads
    : (num_local_app_threads - 1);
  int32_t num_local_bg_threads = table_group_config.num_local_bg_threads;
  int32_t num_total_bg_threads = table_group_config.num_total_bg_threads;
  int32_t num_tables = table_group_config.num_tables;
  int32_t num_total_clients = table_group_config.num_total_clients;
  const std::vector<int32_t> &server_ids = table_group_config.server_ids;
  const std::map<int32_t, HostInfo> &host_map = table_group_config.host_map;

  int32_t client_id = table_group_config.client_id;
  int32_t server_ring_size = table_group_config.server_ring_size;
  ConsistencyModel consistency_model = table_group_config.consistency_model;
  int32_t local_id_min = GlobalContext::get_thread_id_min(client_id);
  int32_t local_id_max = GlobalContext::get_thread_id_max(client_id);
  num_app_threads_registered_ = 1;  // init thread is the first one

  // can be Inited after CommBus but must be before everything else
  GlobalContext::Init(num_total_server_threads,
    num_local_server_threads,
    num_local_app_threads,
    num_local_table_threads,
    num_local_bg_threads,
    num_total_bg_threads,
    num_tables,
    num_total_clients,
    server_ids,
    host_map,
    client_id,
    server_ring_size,
    consistency_model,
    table_group_config.aggressive_cpu);

  CommBus *comm_bus = new CommBus(local_id_min, local_id_max, 1);
  GlobalContext::comm_bus = comm_bus;

  int32_t init_thread_id = local_id_min
                           + GlobalContext::kInitThreadIDOffset;
  CommBus::Config comm_config(init_thread_id, CommBus::kNone, "");

  GlobalContext::comm_bus->ThreadRegister(comm_config);

  if (GlobalContext::am_i_name_node_client()) {
    NameNodeThread::Init();
    ServerThreads::Init(local_id_min + 1);
  } else {
    ServerThreads::Init(local_id_min);
  }

  BgWorkers::Init(&tables_);

  ThreadContext::RegisterThread(init_thread_id);

  if (table_access)
    vector_clock_.AddClock(init_thread_id, 0);

  if (table_group_config.aggressive_clock)
    ClockInternal = ClockAggressive;
  else
    ClockInternal = ClockConservative;

  return init_thread_id;
}

void TableGroup::ShutDown() {
  pthread_barrier_destroy(&register_barrier_);
  BgWorkers::ThreadDeregister();
  ServerThreads::ShutDown();

  if (GlobalContext::am_i_name_node_client())
    NameNodeThread::ShutDown();

  BgWorkers::ShutDown();
  GlobalContext::comm_bus->ThreadDeregister();

  delete GlobalContext::comm_bus;
  for(auto iter = tables_.begin(); iter != tables_.end(); iter++){
    delete iter->second;
  }
  PRINT_STATS();
}

bool TableGroup::CreateTable(int32_t table_id,
  const ClientTableConfig& table_config) {
  TableGroup::max_table_staleness_ = std::max(TableGroup::max_table_staleness_,
      table_config.table_info.table_staleness);
  return BgWorkers::CreateTable(table_id, table_config);
}

void TableGroup::CreateTableDone(){
  BgWorkers::WaitCreateTable();
  pthread_barrier_init(&register_barrier_, 0,
    GlobalContext::get_num_table_threads());
}

void TableGroup::WaitThreadRegister(){
  VLOG(0) << "num_table_threads = "
	  << GlobalContext::get_num_table_threads()
	  << " num_app_threads = "
	  << GlobalContext::get_num_app_threads();

  if (GlobalContext::get_num_table_threads() ==
      GlobalContext::get_num_app_threads()) {
    VLOG(0) << "Init thread WaitThreadRegister";
    pthread_barrier_wait(&register_barrier_);
  }
}

int32_t TableGroup::RegisterThread(){
  REGISTER_THREAD_FOR_STATS(true);
  int app_thread_id_offset = num_app_threads_registered_++;

  int32_t thread_id = GlobalContext::get_local_id_min()
    + GlobalContext::kInitThreadIDOffset + app_thread_id_offset;

  petuum::CommBus::Config comm_config(thread_id, petuum::CommBus::kNone, "");
  GlobalContext::comm_bus->ThreadRegister(comm_config);

  ThreadContext::RegisterThread(thread_id);

  BgWorkers::ThreadRegister();
  vector_clock_.AddClock(thread_id, 0);

  pthread_barrier_wait(&register_barrier_);
  return thread_id;
}

void TableGroup::DeregisterThread(){
  FINALIZE_STATS();
  BgWorkers::ThreadDeregister();
  GlobalContext::comm_bus->ThreadDeregister();
}

void TableGroup::Clock() {
  ThreadContext::Clock();
  TIMER_BEGIN(0, CLOCK);
  ClockInternal();
  TIMER_END(0, CLOCK);
}

void TableGroup::GlobalBarrier() {
  for (int i = 0; i < TableGroup::max_table_staleness_ + 1; ++i) {
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
    VLOG(0) << "ClockAllTables() clock = " << clock;
    BgWorkers::ClockAllTables();
  }
}
}
