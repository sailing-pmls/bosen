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
#include <petuum_ps/consistency/ssp_push_consistency_controller.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps/thread/bg_workers.hpp>
#include <petuum_ps_common/storage/process_storage.hpp>
#include <petuum_ps_common/util/stats.hpp>
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
  SSPConsistencyController(info, table_id, process_storage, oplog,
    sample_row, thread_cache, oplog_index) { }

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
  STATS_APP_SAMPLE_SSP_GET_BEGIN(table_id_);

  // Look for row_id in process_storage_.
  int32_t stalest_clock = std::max(0, ThreadContext::get_clock() - staleness_);

  if(BgWorkers::GetSystemClock() < stalest_clock) {
    STATS_APP_ACCUM_SSPPUSH_GET_COMM_BLOCK_BEGIN(table_id_);
    BgWorkers::WaitSystemClock(stalest_clock);
    STATS_APP_ACCUM_SSPPUSH_GET_COMM_BLOCK_END(table_id_);
  }

  if (process_storage_.Find(row_id, row_accessor)) {
    STATS_APP_SAMPLE_SSP_GET_END(table_id_, true);
    return;
  }

  // Didn't find row_id that's fresh enough in process_storage_.
  // Fetch from server.
  bool found = false;
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

  //LOG(INFO) << "cache missing row id = " << row_id;
  STATS_APP_SAMPLE_SSP_GET_END(table_id_, false);
}

void SSPPushConsistencyController::ThreadGet(int32_t row_id,
  ThreadRowAccessor* row_accessor) {
  STATS_APP_SAMPLE_THREAD_GET_BEGIN(table_id_);

  // Look for row_id in process_storage_.
  int32_t stalest_clock = std::max(0, ThreadContext::get_clock() - staleness_);

  if(BgWorkers::GetSystemClock() < stalest_clock) {
    STATS_APP_ACCUM_SSPPUSH_GET_COMM_BLOCK_BEGIN(table_id_);
    BgWorkers::WaitSystemClock(stalest_clock);
    STATS_APP_ACCUM_SSPPUSH_GET_COMM_BLOCK_END(table_id_);
  }

  AbstractRow *row_data = thread_cache_->GetRow(row_id);
  if (row_data != 0) {
    row_accessor->row_data_ptr_ = row_data;
    STATS_APP_SAMPLE_THREAD_GET_END(table_id_);
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
      //VLOG(0) << "request row " << row_id;
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
  STATS_APP_SAMPLE_THREAD_GET_END(table_id_);
}

}   // namespace petuum
