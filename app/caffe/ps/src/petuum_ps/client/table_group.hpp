/*
 * table_group.hpp
 * author: jinliang
 */

#pragma once

#include <map>
#include <cstdint>

#include <petuum_ps_common/include/configs.hpp>
#include <petuum_ps_common/include/table.hpp>
#include <petuum_ps_common/include/abstract_row.hpp>
#include <petuum_ps_common/util/class_register.hpp>
#include <petuum_ps/client/client_table.hpp>

#include <petuum_ps_common/client/abstract_table_group.hpp>

namespace petuum {

class TableGroup : public AbstractTableGroup {
public:
  TableGroup(const TableGroupConfig &table_group_config,
             bool table_access, int32_t *init_thread_id);

  ~TableGroup();

  bool CreateTable(int32_t table_id,
      const ClientTableConfig& table_config);

  void CreateTableDone();

  void WaitThreadRegister();

  AbstractClientTable *GetTableOrDie(int32_t table_id) {
    auto iter = tables_.find(table_id);
    CHECK(iter != tables_.end()) << "Table " << table_id << " does not exist";
    return static_cast<AbstractClientTable*>(iter->second);
  }

  int32_t RegisterThread();

  void DeregisterThread();

  void Clock();

  void GlobalBarrier();

  void TurnOnEarlyComm();

  void TurnOffEarlyComm();

private:
  typedef void (TableGroup::*ClockFunc) ();
  ClockFunc ClockInternal;

  void ClockAggressive();
  void ClockConservative();

  std::map<int32_t, ClientTable* > tables_;
  pthread_barrier_t register_barrier_;
  std::atomic<int> num_app_threads_registered_;

  // Max staleness among all tables.
  int32_t max_table_staleness_;
  VectorClockMT vector_clock_;
};

}   // namespace petuum
