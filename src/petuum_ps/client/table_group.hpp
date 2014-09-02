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
