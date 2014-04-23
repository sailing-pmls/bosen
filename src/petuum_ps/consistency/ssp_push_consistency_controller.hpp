#pragma once

#include "petuum_ps/consistency/abstract_consistency_controller.hpp"
#include "petuum_ps/oplog/oplog.hpp"
#include "petuum_ps/util/vector_clock_mt.hpp"
#include <boost/thread/tss.hpp>
#include <utility>
#include <vector>
#include <cstdint>
#include <atomic>

namespace petuum {

class SSPPushConsistencyController : public AbstractConsistencyController {
public:
  SSPPushConsistencyController(const TableInfo& info,
    int32_t table_id,
    ProcessStorage& process_storage,
    TableOpLog& oplog,
    const AbstractRow* sample_row,
    boost::thread_specific_ptr<ThreadTable> &thread_cache,
    TableOpLogIndex &oplog_index);

  void GetAsync(int32_t row_id);
  void WaitPendingAsnycGet();

  // Check freshness; make request and block if too stale or row_id not found
  // in storage.
  void Get(int32_t row_id, RowAccessor* row_accessor);

  // Return immediately.
  void Inc(int32_t row_id, int32_t column_id, const void* delta);

  void BatchInc(int32_t row_id, const int32_t* column_ids, const void* updates,
    int32_t num_updates);

  void ThreadGet(int32_t row_id, ThreadRowAccessor* row_accessor);

  void ThreadInc(int32_t row_id, int32_t column_id, const void* delta);

  // Increment column_ids.size() entries of a row. deltas points to an array.
  void ThreadBatchInc(int32_t row_id, const int32_t* column_ids,
    const void* updates, int32_t num_updates);


  void Clock();

private:  // private methods
  // Fetch row_id from process_storage_ and check freshness against
  // stalest_iter. Return true if found and fresh, false otherwise.
  bool FetchFreshFromProcessStorage(int32_t row_id, int32_t stalest_iter,
      RowAccessor* row_accessor);

private:
  int32_t table_id_;

  // SSP staleness parameter.
  int32_t staleness_;

  static const size_t kMaxPendingAsyncGetCnt = 256;
  boost::thread_specific_ptr<size_t> pending_async_get_cnt_;
};

}  // namespace petuum
