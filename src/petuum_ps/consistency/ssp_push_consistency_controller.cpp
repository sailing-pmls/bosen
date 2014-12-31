#include <petuum_ps/consistency/ssp_push_consistency_controller.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps/thread/bg_workers.hpp>
#include <petuum_ps_common/util/stats.hpp>
#include <glog/logging.h>

namespace petuum {

SSPPushConsistencyController::SSPPushConsistencyController(
  const TableInfo& info,
  int32_t table_id,
  AbstractProcessStorage& process_storage,
  AbstractOpLog& oplog,
  const AbstractRow* sample_row,
  boost::thread_specific_ptr<ThreadTable> &thread_cache,
  TableOpLogIndex &oplog_index,
  int32_t row_oplog_type) :
  SSPConsistencyController(info, table_id, process_storage, oplog,
                           sample_row, thread_cache, oplog_index, row_oplog_type) { }

void SSPPushConsistencyController::GetAsyncForced(int32_t row_id) {
  // Look for row_id in process_storage_.
  int32_t stalest_clock = ThreadContext::get_clock();

  if (pending_async_get_cnt_.get() == 0) {
    pending_async_get_cnt_.reset(new size_t);
    *pending_async_get_cnt_ = 0;
  }

  if (*pending_async_get_cnt_ == kMaxPendingAsyncGetCnt) {
    BgWorkers::GetAsyncRowRequestReply();
    *pending_async_get_cnt_ -= 1;
  }

  BgWorkers::RequestRowAsync(table_id_, row_id, stalest_clock, true);
  *pending_async_get_cnt_ += 1;
}

void SSPPushConsistencyController::GetAsync(int32_t row_id) {
  // Look for row_id in process_storage_.
  int32_t stalest_clock = ThreadContext::get_clock();

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

  BgWorkers::RequestRowAsync(table_id_, row_id, stalest_clock, false);
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

ClientRow *SSPPushConsistencyController::Get(int32_t row_id,
  RowAccessor* row_accessor) {
  STATS_APP_SAMPLE_SSP_GET_BEGIN(table_id_);

  // Look for row_id in process_storage_.
  int32_t stalest_clock = std::max(0, ThreadContext::get_clock() - staleness_);

  if (ThreadContext::GetCachedSystemClock() < stalest_clock) {
    int32_t system_clock = BgWorkers::GetSystemClock();
    if(system_clock < stalest_clock) {
      STATS_APP_ACCUM_SSPPUSH_GET_COMM_BLOCK_BEGIN(table_id_);
      BgWorkers::WaitSystemClock(stalest_clock);
      STATS_APP_ACCUM_SSPPUSH_GET_COMM_BLOCK_END(table_id_);
      system_clock = BgWorkers::GetSystemClock();
    }
    ThreadContext::SetCachedSystemClock(system_clock);
  }

  ClientRow *client_row = process_storage_.Find(row_id, row_accessor);

  if (client_row != 0) {
    STATS_APP_SAMPLE_SSP_GET_END(table_id_, true);
    return client_row;
  }

  // Didn't find row_id that's fresh enough in process_storage_.
  // Fetch from server.
   int32_t num_fetches = 0;
  do {
    STATS_APP_ACCUM_SSP_GET_SERVER_FETCH_BEGIN(table_id_);
    BgWorkers::RequestRow(table_id_, row_id, stalest_clock);
    STATS_APP_ACCUM_SSP_GET_SERVER_FETCH_END(table_id_);

    // fetch again
    client_row = process_storage_.Find(row_id, row_accessor);
    // TODO (jinliang):
    // It's possible that the application thread does not find the row that
    // the bg thread has just inserted. In practice, this shouldn't be an issue.
    // We'll fix it if it turns out there are too many misses.
    ++num_fetches;
    CHECK_LE(num_fetches, 3); // to prevent infinite loop
  }while(client_row == 0);

  STATS_APP_SAMPLE_SSP_GET_END(table_id_, false);
  return client_row;
}

void SSPPushConsistencyController::ThreadGet(int32_t row_id,
  ThreadRowAccessor* row_accessor) {
  STATS_APP_SAMPLE_THREAD_GET_BEGIN(table_id_);

  // Look for row_id in process_storage_.
  int32_t stalest_clock = std::max(0, ThreadContext::get_clock() - staleness_);

  if (ThreadContext::GetCachedSystemClock() < stalest_clock) {
    int32_t system_clock = BgWorkers::GetSystemClock();
    if(system_clock < stalest_clock) {
      STATS_APP_ACCUM_SSPPUSH_GET_COMM_BLOCK_BEGIN(table_id_);
      BgWorkers::WaitSystemClock(stalest_clock);
      STATS_APP_ACCUM_SSPPUSH_GET_COMM_BLOCK_END(table_id_);
      system_clock = BgWorkers::GetSystemClock();
    }
    ThreadContext::SetCachedSystemClock(system_clock);
  }

  AbstractRow *row_data = thread_cache_->GetRow(row_id);
  if (row_data != 0) {
    row_accessor->row_data_ptr_ = row_data;
    STATS_APP_SAMPLE_THREAD_GET_END(table_id_);
    return;
  }

  RowAccessor process_row_accessor;
  ClientRow *client_row = process_storage_.Find(row_id, &process_row_accessor);
  if (client_row == 0) {
    // Didn't find row_id that's fresh enough in process_storage_.
    // Fetch from server.
    int32_t num_fetches = 0;
    do {
      BgWorkers::RequestRow(table_id_, row_id, stalest_clock);
      // fetch again
      client_row = process_storage_.Find(row_id, &process_row_accessor);
      // TODO (jinliang):
      // It's possible that the application thread does not find the row that
      // the bg thread has just inserted. In practice, this shouldn't be an issue.
      // We'll fix it if it turns out there are too many misses.
      ++num_fetches;
      CHECK_LE(num_fetches, 3); // to prevent infinite loop
    }while(client_row == 0);
  }
  AbstractRow *tmp_row_data = client_row->GetRowDataPtr();
  thread_cache_->InsertRow(row_id, tmp_row_data);
  row_data = thread_cache_->GetRow(row_id);
  CHECK(row_data != 0);
  row_accessor->row_data_ptr_ = row_data;
  STATS_APP_SAMPLE_THREAD_GET_END(table_id_);
}

}   // namespace petuum
