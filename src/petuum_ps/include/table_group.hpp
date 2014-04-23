/*
 * table_group.hpp
 * author: jinliang
 */

#pragma once

#include <map>

#include "petuum_ps/include/configs.hpp"
#include "petuum_ps/include/table.hpp"
#include "petuum_ps/include/abstract_row.hpp"
#include "petuum_ps/util/class_register.hpp"
#include <cstdint>


namespace petuum {

class TableGroup {
public:
  // Can be called only once per process. Must be called after RegisterRow() to
  // be a barrier for CreateTable(), and "happen before" any other call to
  // TableGroup. The thread that calls Init() is refered to as the init
  // thread.  If the init thread needs to access table API (e.g., init thread
  // itself being a worker thread), it should set table_access to true. Init
  // thread is responsible for calling RegisterRow(), CreateTable() and
  // ShutDown(). Calling those functions from other threads is not allowed.
  // Init thread does not need to DeregisterThread() nor RegisterThread().
  static int Init(const TableGroupConfig &table_group_config,
    bool table_access);

  // Init thread need to call ShutDown() after all other app threads have
  // deregistered. Any other call to TableGroup and Table API must return
  // before calling ShutDown().
  static void ShutDown();

  // Should be called before Init(). Not thread-safe. We strongly recommend to
  // call RegisterRow from init thread to avoid race condition.
  template<typename ROW>
  static void RegisterRow(int32_t row_type) {
    ClassRegistry<AbstractRow>::GetRegistry().AddCreator(row_type,
      CreateObj<AbstractRow, ROW>);
  }

  static bool CreateTable(int32_t table_id,
      const ClientTableConfig& table_config);

  // Must be called by Init thread after creating all tables and before any
  // other thread calls RegisterThread().
  static void CreateTableDone();

  // Called by Init thread only before it access any table API.
  // Must be called after CreateTableDone().
  // If Init thread does not access table API, it makes no difference calling
  // this function.
  static void WaitThreadRegister();

  // GetTableOrDie is thread-safe with respect to other calls to
  // GetTableOrDie() Getter, terminate if table is not found.
  template<typename UPDATE>
  static Table<UPDATE> GetTableOrDie(int32_t table_id) {
    auto iter = tables_.find(table_id);
    CHECK(iter != tables_.end()) << "Table " << table_id << " does not exist";
    iter->second->RegisterThread();
    return Table<UPDATE>(iter->second);
  }

  // A app threads except init thread should register itself before accessing
  // any Table API. In SSP mode, if a thread invokes RegisterThread with
  // true, its clock will be kept track of, so it should call Clock()
  // properly.
  static int32_t RegisterThread();

  // A registered thread must deregister itself.
  static void DeregisterThread();

  // Advance clock for the application thread.
  //
  // Comment(wdai): We only use one vector clock per process, each clock for a
  // registered app thread. The vector clock is not associated with individual
  // tables.
  static void Clock();

  // Called by application threads that access table API
  // (referred to as table threads).
  // Threads that calls GlobalBarrier must be at the same clock.
  // 1) A table thread may not go beyond the barrier until all table threads
  // have reached the barrier;
  // 2) Table threads that move beyond the barrier are guaranteed to see
  // the updates that other table threads apply to the table.
  static void GlobalBarrier();

private:
  typedef void (*ClockFunc)();
  static ClockFunc ClockInternal;

  static void ClockAggressive();
  static void ClockConservative();

  static std::map<int32_t, ClientTable* > tables_;
  static pthread_barrier_t register_barrier_;
  static std::atomic<int> num_app_threads_registered_;

  // Max staleness among all tables.
  static int32_t max_table_staleness_;
  static VectorClockMT vector_clock_;

};

}   // namespace petuum
