
#include "petuum_ps/consistency/ssp_consistency_controller.hpp"
#include "petuum_ps/storage/process_storage.hpp"
#include "petuum_ps/thread/context.hpp"
#include "petuum_ps/thread/bg_workers.hpp"
#include "petuum_ps/util/stats.hpp"
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
  AbstractConsistencyController(info, table_id, process_storage, oplog,
    sample_row, thread_cache, oplog_index),
  table_id_(table_id),
  staleness_(info.table_staleness) { }

void SSPConsistencyController::Get(int32_t row_id, RowAccessor* row_accessor) {
  // Look for row_id in process_storage_.
  int32_t stalest_clock = std::max(0, ThreadContext::get_clock() - staleness_);

  TIMER_BEGIN(table_id_, SSP_PSTORAGE_FIND);
  bool found = process_storage_.Find(row_id, row_accessor);
  TIMER_END(table_id_, SSP_PSTORAGE_FIND);

  if (found) {
    // Found it! Check staleness.
    int32_t clock = row_accessor->GetClientRow()->GetClock();
    if (clock >= stalest_clock) {
      return;
    }
  }

  // Didn't find row_id that's fresh enough in process_storage_.
  // Fetch from server.
  found = false;
  int32_t num_fetches = 0;
  do {
    TIMER_BEGIN(table_id_, SSP_ROW_REQUEST);
    BgWorkers::RequestRow(table_id_, row_id, stalest_clock);
    TIMER_END(table_id_, SSP_ROW_REQUEST);

    // fetch again
    found = process_storage_.Find(row_id, row_accessor);
    // TODO (jinliang):
    // It's possible that the application thread does not find the row that
    // the bg thread has just inserted. In practice, this shouldn't be an issue.
    // We'll fix it if it turns out there are too many misses.
    ++num_fetches;
    CHECK_LE(num_fetches, 3); // to prevent infinite loop
  }while(!found);

  CHECK_GE(row_accessor->GetClientRow()->GetClock(),
    stalest_clock);
}

void SSPConsistencyController::Inc(int32_t row_id, int32_t column_id,
    const void* delta) {

  thread_cache_->IndexUpdate(row_id);

  oplog_.Inc(row_id,column_id, delta);

  RowAccessor row_accessor;
  bool found = process_storage_.Find(row_id, &row_accessor);
  if (found) {
    row_accessor.GetRowData()->ApplyInc(column_id, delta);
  }
}

void SSPConsistencyController::BatchInc(int32_t row_id,
  const int32_t* column_ids, const void* updates, int32_t num_updates) {

  thread_cache_->IndexUpdate(row_id);
  oplog_.BatchInc(row_id, column_ids, updates, num_updates);

  RowAccessor row_accessor;
  bool found = process_storage_.Find(row_id, &row_accessor);
  if (found) {
    row_accessor.GetRowData()->ApplyBatchInc(column_ids, updates,
                                             num_updates);
  }
}

void SSPConsistencyController::ThreadGet(int32_t row_id,
  ThreadRowAccessor* row_accessor) {

  AbstractRow *row_data = thread_cache_->GetRow(row_id);
  if (row_data != 0) {
    row_accessor->row_data_ptr_ = row_data;
    return;
  }

  RowAccessor process_row_accessor;
  TIMER_BEGIN(table_id_, SSP_PSTORAGE_FIND);
  bool found = process_storage_.Find(row_id, &process_row_accessor);
  TIMER_END(table_id_, SSP_PSTORAGE_FIND);

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
      return;
    }
  }

  // Didn't find row_id that's fresh enough in process_storage_.
  // Fetch from server.
  found = false;
  int32_t num_fetches = 0;
  do {
    TIMER_BEGIN(table_id_, SSP_ROW_REQUEST);
    BgWorkers::RequestRow(table_id_, row_id, stalest_clock);
    TIMER_END(table_id_, SSP_ROW_REQUEST);

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
}

void SSPConsistencyController::ThreadInc(int32_t row_id, int32_t column_id,
    const void* delta) {
  thread_cache_->Inc(row_id, column_id, delta);
}

void SSPConsistencyController::ThreadBatchInc(int32_t row_id,
  const int32_t* column_ids, const void* updates, int32_t num_updates) {
  thread_cache_->BatchInc(row_id, column_ids, updates, num_updates);
}

void SSPConsistencyController::Clock() {
  // order is important
  thread_cache_->FlushCache(process_storage_, oplog_);
  thread_cache_->FlushOpLogIndex(oplog_index_);
}

}   // namespace petuum
