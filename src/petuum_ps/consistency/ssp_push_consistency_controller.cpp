#include "petuum_ps/consistency/ssp_push_consistency_controller.hpp"
#include "petuum_ps/storage/process_storage.hpp"
#include "petuum_ps/thread/context.hpp"
#include "petuum_ps/thread/bg_workers.hpp"
#include "petuum_ps/util/stats.hpp"
#include <glog/logging.h>

namespace petuum {

SSPPushConsistencyController::SSPPushConsistencyController(
  const TableInfo& info,
  int32_t table_id,
  ProcessStorage& process_storage,
  TableOpLog& oplog,
  const AbstractRow* sample_row,
  boost::thread_specific_ptr<ThreadTable> &thread_cache,
  TableOpLogIndex &oplog_index) :
  AbstractConsistencyController(info, table_id, process_storage, oplog,
    sample_row, thread_cache, oplog_index),
  table_id_(table_id),
  staleness_(info.table_staleness){ }

void SSPPushConsistencyController::GetAsync(int32_t row_id) {
  // Look for row_id in process_storage_.
  int32_t stalest_clock = BgWorkers::GetSystemClock();

  if (process_storage_.Find(row_id))
    return;

  if (pending_async_get_cnt_.get() == 0) {
    pending_async_get_cnt_.reset(new size_t);
    *pending_async_get_cnt_ = 0;
  }

  if (*pending_async_get_cnt_ == kMaxPendingAsyncGetCnt) {
    BgWorkers::GetAsyncRowRequestReply();
    *pending_async_get_cnt_ -= 1;
  }

  BgWorkers::RequestRowAsync(table_id_, row_id, stalest_clock);
  *pending_async_get_cnt_ += 1;
}

void SSPPushConsistencyController::WaitPendingAsnycGet() {
  if (pending_async_get_cnt_.get() == 0)
    return;

  while(*pending_async_get_cnt_ > 0) {
    BgWorkers::GetAsyncRowRequestReply();
    *pending_async_get_cnt_ -= 1;
  }
}

void SSPPushConsistencyController::Get(int32_t row_id,
  RowAccessor* row_accessor) {
  // Look for row_id in process_storage_.
  int32_t stalest_clock = std::max(0, ThreadContext::get_clock() - staleness_);

  if(BgWorkers::GetSystemClock() < stalest_clock)
    BgWorkers::WaitSystemClock(stalest_clock);

  if (process_storage_.Find(row_id, row_accessor)) {
    //VLOG(0) << "Find " << row_id << " and return";
    return;
  }

  // Didn't find row_id that's fresh enough in process_storage_.
  // Fetch from server.
  bool found = false;
  int32_t num_fetches = 0;
  do {
    BgWorkers::RequestRow(table_id_, row_id, stalest_clock);
    VLOG_EVERY_N(0, 100) << "request row " << row_id;
    // fetch again
    found = process_storage_.Find(row_id, row_accessor);
    // TODO (jinliang):
    // It's possible that the application thread does not find the row that
    // the bg thread has just inserted. In practice, this shouldn't be an issue.
    // We'll fix it if it turns out there are too many misses.
    ++num_fetches;
    CHECK_LE(num_fetches, 3); // to prevent infinite loop
  }while(!found);
}

void SSPPushConsistencyController::Inc(int32_t row_id, int32_t column_id,
    const void* delta) {

  thread_cache_->IndexUpdate(row_id);

  oplog_.Inc(row_id,column_id, delta);

  RowAccessor row_accessor;
  bool found = process_storage_.Find(row_id, &row_accessor);
  if (found) {
    row_accessor.GetRowData()->ApplyInc(column_id, delta);
  }
}

void SSPPushConsistencyController::BatchInc(int32_t row_id,
  const int32_t* column_ids, const void* updates, int32_t num_updates) {

  TIMER_BEGIN(table_id_, SSPPUSH_BATCH_INC_THR_UPDATE_INDEX);
  thread_cache_->IndexUpdate(row_id);
  TIMER_END(table_id_, SSPPUSH_BATCH_INC_THR_UPDATE_INDEX);

  TIMER_BEGIN(table_id_, SSPPUSH_BATCH_INC_OPLOG);
  oplog_.BatchInc(row_id, column_ids, updates, num_updates);
  TIMER_END(table_id_, SSPPUSH_BATCH_INC_OPLOG);

  TIMER_BEGIN(table_id_, SSPPUSH_BATCH_INC_PROCESS_STORAGE);
  RowAccessor row_accessor;
  bool found = process_storage_.Find(row_id, &row_accessor);
  if (found) {
    row_accessor.GetRowData()->ApplyBatchInc(column_ids, updates,
						 num_updates);
  }
  TIMER_END(table_id_, SSPPUSH_BATCH_INC_PROCESS_STORAGE);
}

void SSPPushConsistencyController::ThreadGet(int32_t row_id,
  ThreadRowAccessor* row_accessor) {
  // Look for row_id in process_storage_.
  int32_t stalest_clock = std::max(0, ThreadContext::get_clock() - staleness_);

  if(BgWorkers::GetSystemClock() < stalest_clock)
    BgWorkers::WaitSystemClock(stalest_clock);

  AbstractRow *row_data = thread_cache_->GetRow(row_id);
  if (row_data != 0) {
    row_accessor->row_data_ptr_ = row_data;
    return;
  }

  RowAccessor process_row_accessor;

  if (!process_storage_.Find(row_id, &process_row_accessor)) {
    // Didn't find row_id that's fresh enough in process_storage_.
    // Fetch from server.
    bool found = false;
    int32_t num_fetches = 0;
    do {
      BgWorkers::RequestRow(table_id_, row_id, stalest_clock);
      VLOG(0) << "request row " << row_id;
      // fetch again
      found = process_storage_.Find(row_id, &process_row_accessor);
      // TODO (jinliang):
      // It's possible that the application thread does not find the row that
      // the bg thread has just inserted. In practice, this shouldn't be an issue.
      // We'll fix it if it turns out there are too many misses.
      ++num_fetches;
      CHECK_LE(num_fetches, 3); // to prevent infinite loop
    }while(!found);
  }
  AbstractRow *tmp_row_data = process_row_accessor.GetRowData();
  thread_cache_->InsertRow(row_id, tmp_row_data);
  row_data = thread_cache_->GetRow(row_id);
  CHECK(row_data != 0);
  row_accessor->row_data_ptr_ = row_data;
}

void SSPPushConsistencyController::ThreadInc(int32_t row_id, int32_t column_id,
    const void* delta) {
  thread_cache_->Inc(row_id, column_id, delta);
}

void SSPPushConsistencyController::ThreadBatchInc(int32_t row_id,
  const int32_t* column_ids, const void* updates, int32_t num_updates) {
  TIMER_BEGIN(table_id_, SSPPUSH_THREAD_BATCH_INC);
  thread_cache_->BatchInc(row_id, column_ids, updates, num_updates);
  TIMER_END(table_id_, SSPPUSH_THREAD_BATCH_INC);
}

void SSPPushConsistencyController::Clock() {
  // order is important
  thread_cache_->FlushCache(process_storage_, oplog_);
  thread_cache_->FlushOpLogIndex(oplog_index_);
}

}   // namespace petuum
