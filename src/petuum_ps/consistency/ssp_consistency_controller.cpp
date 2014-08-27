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

#include <petuum_ps/consistency/ssp_consistency_controller.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps/thread/bg_workers.hpp>
#include <petuum_ps_common/storage/process_storage.hpp>
#include <petuum_ps_common/util/stats.hpp>
#include <glog/logging.h>
#include <algorithm>

namespace petuum {

SSPConsistencyController::SSPConsistencyController(const TableInfo& info,
  int32_t table_id,
  ProcessStorage& process_storage,
  TableOpLog& oplog,
  const AbstractRow* sample_row,
  boost::thread_specific_ptr<ThreadTable> &thread_cache,
  TableOpLogIndex &oplog_index) :
  AbstractConsistencyController(table_id, process_storage,
    sample_row),
  staleness_(info.table_staleness),
  thread_cache_(thread_cache),
  oplog_index_(oplog_index),
  oplog_(oplog) { }

void SSPConsistencyController::Get(int32_t row_id, RowAccessor* row_accessor) {
  STATS_APP_SAMPLE_SSP_GET_BEGIN(table_id_);

  // Look for row_id in process_storage_.
  int32_t stalest_clock = std::max(0, ThreadContext::get_clock() - staleness_);

  bool found = process_storage_.Find(row_id, row_accessor);

  if (found) {
    // Found it! Check staleness.
    int32_t clock = row_accessor->GetClientRow()->GetClock();
    if (clock >= stalest_clock) {
      STATS_APP_SAMPLE_SSP_GET_END(table_id_, true);
      return;
    }
  }

  // Didn't find row_id that's fresh enough in process_storage_.
  // Fetch from server.
  found = false;
  int32_t num_fetches = 0;
  do {
    STATS_APP_ACCUM_SSP_GET_SERVER_FETCH_BEGIN(table_id_);
    BgWorkers::RequestRow(table_id_, row_id, stalest_clock);
    STATS_APP_ACCUM_SSP_GET_SERVER_FETCH_END(table_id_);

    // fetch again
    found = process_storage_.Find(row_id, row_accessor);
    // TODO (jinliang):
    // It's possible that the application thread does not find the row that
    // the bg thread has just inserted. In practice, this shouldn't be an issue.
    // We'll fix it if it turns out there are too many misses.
    ++num_fetches;
    CHECK_LE(num_fetches, 3); // to prevent infinite loop
  }while(!found);

  CHECK_GE(row_accessor->GetClientRow()->GetClock(), stalest_clock);
  STATS_APP_SAMPLE_SSP_GET_END(table_id_, false);
}

void SSPConsistencyController::Inc(int32_t row_id, int32_t column_id,
    const void* delta) {
  thread_cache_->IndexUpdate(row_id);

  OpLogAccessor oplog_accessor;
  oplog_.FindInsertOpLog(row_id, &oplog_accessor);
  void *oplog_delta = oplog_accessor.FindCreate(column_id);
  sample_row_->AddUpdates(column_id, oplog_delta, delta);

  RowAccessor row_accessor;
  bool found = process_storage_.Find(row_id, &row_accessor);
  if (found) {
    row_accessor.GetRowData()->ApplyInc(column_id, delta);
  }
}

void SSPConsistencyController::BatchInc(int32_t row_id,
  const int32_t* column_ids, const void* updates, int32_t num_updates) {

  thread_cache_->IndexUpdate(row_id);

  OpLogAccessor oplog_accessor;
  oplog_.FindInsertOpLog(row_id, &oplog_accessor);

  const uint8_t* deltas_uint8 = reinterpret_cast<const uint8_t*>(updates);

  for (int i = 0; i < num_updates; ++i) {
    void *oplog_delta = oplog_accessor.FindCreate(column_ids[i]);
    sample_row_->AddUpdates(column_ids[i], oplog_delta, deltas_uint8
			    + sample_row_->get_update_size()*i);
  }

  RowAccessor row_accessor;
  bool found = process_storage_.Find(row_id, &row_accessor);
  if (found) {
    row_accessor.GetRowData()->ApplyBatchInc(column_ids, updates,
                                             num_updates);
  }
}

void SSPConsistencyController::ThreadGet(int32_t row_id,
  ThreadRowAccessor* row_accessor) {
  STATS_APP_SAMPLE_THREAD_GET_BEGIN(table_id_);
  AbstractRow *row_data = thread_cache_->GetRow(row_id);
  if (row_data != 0) {
    row_accessor->row_data_ptr_ = row_data;
    return;
  }

  RowAccessor process_row_accessor;
  bool found = process_storage_.Find(row_id, &process_row_accessor);

  int32_t stalest_clock = std::max(0, ThreadContext::get_clock() - staleness_);
  if (found) {
    // Found it! Check staleness.
    int32_t clock = process_row_accessor.GetClientRow()->GetClock();
    if (clock >= stalest_clock) {
      AbstractRow *tmp_row_data = process_row_accessor.GetRowData();
      thread_cache_->InsertRow(row_id, tmp_row_data);
      row_data = thread_cache_->GetRow(row_id);
      CHECK(row_data != 0);
      row_accessor->row_data_ptr_ = row_data;
      STATS_APP_SAMPLE_THREAD_GET_END(table_id_);
      return;
    }
  }

  // Didn't find row_id that's fresh enough in process_storage_.
  // Fetch from server.
  found = false;
  int32_t num_fetches = 0;
  do {
    BgWorkers::RequestRow(table_id_, row_id, stalest_clock);

    // fetch again
    found = process_storage_.Find(row_id, &process_row_accessor);
    // TODO (jinliang):
    // It's possible that the application thread does not find the row that
    // the bg thread has just inserted. In practice, this shouldn't be an issue.
    // We'll fix it if it turns out there are too many misses.
    ++num_fetches;
    CHECK_LE(num_fetches, 3); // to prevent infinite loop
  }while(!found);

  CHECK_GE(process_row_accessor.GetClientRow()->GetClock(),
    stalest_clock);

  AbstractRow *tmp_row_data = process_row_accessor.GetRowData();
  thread_cache_->InsertRow(row_id, tmp_row_data);
  row_data = thread_cache_->GetRow(row_id);
  CHECK(row_data != 0);
  row_accessor->row_data_ptr_ = row_data;
  STATS_APP_SAMPLE_THREAD_GET_END(table_id_);
}

void SSPConsistencyController::ThreadInc(int32_t row_id, int32_t column_id,
    const void* delta) {
  thread_cache_->Inc(row_id, column_id, delta);
}

void SSPConsistencyController::ThreadBatchInc(int32_t row_id,
  const int32_t* column_ids, const void* updates, int32_t num_updates) {
  thread_cache_->BatchInc(row_id, column_ids, updates, num_updates);
}

void SSPConsistencyController::FlushThreadCache() {
  thread_cache_->FlushCache(process_storage_, oplog_, sample_row_);
}

void SSPConsistencyController::Clock() {
  // order is important
  thread_cache_->FlushCache(process_storage_, oplog_, sample_row_);
  thread_cache_->FlushOpLogIndex(oplog_index_);
}

}   // namespace petuum
