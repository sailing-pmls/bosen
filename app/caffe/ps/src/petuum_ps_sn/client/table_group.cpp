#include <petuum_ps_sn/client/table_group.hpp>
#include <petuum_ps_sn/thread/context.hpp>
#include <petuum_ps_common/util/stats.hpp>

#include <petuum_ps_sn/client/client_table.hpp>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace petuum {
TableGroupSN::TableGroupSN(const TableGroupConfig &table_group_config,
                           bool table_access, int32_t *init_thread_id) {

  int32_t num_local_app_threads = table_group_config.num_local_app_threads;
  int32_t num_local_table_threads = table_access ? num_local_app_threads
    : (num_local_app_threads - 1);
  int32_t num_tables = table_group_config.num_tables;

  int32_t local_id_min = GlobalContextSN::get_local_id_min();
  int32_t local_id_max = GlobalContextSN::get_local_id_max();
  num_app_threads_registered_ = 1;  // init thread is the first one

  STATS_INIT(table_group_config);
  VLOG(0) << "Calling STATS_REGISTER_THREAD";
  STATS_REGISTER_THREAD(kAppThread);

  // can be Inited after CommBus but must be before everything else
  GlobalContextSN::Init(num_local_app_threads,
    num_local_table_threads,
    num_tables,
    table_group_config.ooc_path_prefix,
    table_group_config.consistency_model);

  CommBus *comm_bus = new CommBus(local_id_min, local_id_max, 0);
  GlobalContextSN::comm_bus = comm_bus;

  *init_thread_id = local_id_min
                    + GlobalContextSN::kInitThreadIDOffset;
  CommBus::Config comm_config(*init_thread_id, CommBus::kNone, "");

  GlobalContextSN::comm_bus->ThreadRegister(comm_config);

  ThreadContextSN::RegisterThread(*init_thread_id);

  if (table_access) {
    GlobalContextSN::vector_clock_.AddClock(*init_thread_id, 0);
  }

}

TableGroupSN::~TableGroupSN() {
  pthread_barrier_destroy(&register_barrier_);
  GlobalContextSN::comm_bus->ThreadDeregister();

  delete GlobalContextSN::comm_bus;
  for(auto iter = tables_.begin(); iter != tables_.end(); iter++){
    delete iter->second;
  }
  STATS_DEREGISTER_THREAD();
  STATS_PRINT();
}

bool TableGroupSN::CreateTable(int32_t table_id,
  const ClientTableConfig& table_config) {
  max_table_staleness_ = std::max(max_table_staleness_,
      table_config.table_info.table_staleness);

  ClientTableSN *client_table = new ClientTableSN(table_id, table_config);
  tables_[table_id] = client_table;

  client_table->RegisterThread();
  return true;
}

void TableGroupSN::CreateTableDone() {
  pthread_barrier_init(&register_barrier_, 0,
		       GlobalContextSN::get_num_table_threads());
}

void TableGroupSN::WaitThreadRegister() {
  if (GlobalContextSN::get_num_table_threads() ==
      GlobalContextSN::get_num_app_threads()) {
    pthread_barrier_wait(&register_barrier_);
  }
}

int32_t TableGroupSN::RegisterThread() {
  STATS_REGISTER_THREAD(kAppThread);
  int app_thread_id_offset = num_app_threads_registered_++;

  int32_t thread_id = GlobalContextSN::get_local_id_min()
    + GlobalContextSN::kInitThreadIDOffset + app_thread_id_offset;

  petuum::CommBus::Config comm_config(thread_id, petuum::CommBus::kNone, "");
  GlobalContextSN::comm_bus->ThreadRegister(comm_config);

  ThreadContextSN::RegisterThread(thread_id);

  GlobalContextSN::vector_clock_.AddClock(thread_id, 0);

  for (auto table_iter = tables_.cbegin(); table_iter != tables_.cend();
       table_iter++) {
    table_iter->second->RegisterThread();
  }

  pthread_barrier_wait(&register_barrier_);
  return thread_id;
}

void TableGroupSN::DeregisterThread(){
  GlobalContextSN::comm_bus->ThreadDeregister();
  STATS_DEREGISTER_THREAD();
}

void TableGroupSN::Clock() {
  STATS_APP_ACCUM_TG_CLOCK_BEGIN();

  ThreadContextSN::Clock();
  ThreadContextSN::dec_clock_ahead();
  int32_t clock = GlobalContextSN::vector_clock_.Tick(ThreadContextSN::get_id());

  if (clock != 0) {
    std::lock_guard<std::mutex> lock(GlobalContextSN::clock_mtx_);
    GlobalContextSN::clock_cond_var_.notify_all();
  }

  STATS_APP_ACCUM_TG_CLOCK_END();
}

void TableGroupSN::GlobalBarrier() {
  for (int i = 0; i < max_table_staleness_ + 1; ++i) {
    Clock();
  }
}

}
