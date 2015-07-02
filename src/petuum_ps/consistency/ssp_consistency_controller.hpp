// author: jinliang
#pragma once

#include <petuum_ps_common/consistency/abstract_consistency_controller.hpp>
#include <petuum_ps/oplog/abstract_oplog.hpp>
#include <petuum_ps_common/util/vector_clock_mt.hpp>
#include <petuum_ps/client/thread_table.hpp>
#include <utility>
#include <vector>
#include <cstdint>
#include <atomic>

#include <functional>

namespace petuum {

class SSPConsistencyController : public AbstractConsistencyController {
public:
  SSPConsistencyController(
      const TableInfo& info,
      int32_t table_id,
      AbstractProcessStorage& process_storage,
      AbstractOpLog& oplog,
      const AbstractRow* sample_row,
      boost::thread_specific_ptr<ThreadTable> &thread_cache,
      TableOpLogIndex &oplog_index,
      int32_t row_oplog_type);

  // We don't need GetAsync because in SSP we reply on the clock count of each
  // client row to check whether the row is too stale and fetch it from server
  // when it is too stale.
  virtual void GetAsyncForced(int32_t row_id) { }
  virtual void GetAsync(int32_t row_id) { }
  virtual void WaitPendingAsnycGet() { }

  // Check freshness; make request and block if too stale or row_id not found
  // in storage.
  virtual ClientRow *Get(int32_t row_id, RowAccessor* row_accessor);

  // Return immediately.
  virtual void Inc(int32_t row_id, int32_t column_id, const void* delta);

  virtual void BatchInc(int32_t row_id, const int32_t* column_ids, const void* updates,
			int32_t num_updates);

  virtual void DenseBatchInc(int32_t row_id, const void *updates,
                             int32_t index_st, int32_t num_updates);

  virtual void ThreadGet(int32_t row_id, ThreadRowAccessor* row_accessor);

  virtual void ThreadInc(int32_t row_id, int32_t column_id, const void* delta);

  // Increment column_ids.size() entries of a row. deltas points to an array.
  virtual void ThreadBatchInc(int32_t row_id, const int32_t* column_ids,
			      const void* updates, int32_t num_updates);

  virtual void ThreadDenseBatchInc(
      int32_t row_id, const void *updates, int32_t index_st,
      int32_t num_updates);

  virtual void FlushThreadCache();
  virtual void Clock();

protected:
  void DenseBatchIncDenseOpLog(OpLogAccessor *oplog_accessor, const uint8_t *updates,
                               int32_t index_st, int32_t num_updates);
  void DenseBatchIncNonDenseOpLog(OpLogAccessor *oplog_accessor, const uint8_t *updates,
                                  int32_t index_st, int32_t num_updates);
  typedef void (SSPConsistencyController::*DenseBatchIncOpLogFunc)(
      OpLogAccessor *oplog_accessor, const uint8_t *updates,
      int32_t index_st, int32_t num_updates);

  // SSP staleness parameter.
  int32_t staleness_;

  boost::thread_specific_ptr<ThreadTable> &thread_cache_;
  TableOpLogIndex &oplog_index_;

  // Controller will only write to oplog_ but never read from it, as
  // all local updates are reflected in the row values.
  AbstractOpLog& oplog_;

  typedef std::function<void(int32_t, void*, const void*)> AddUpdatesFunc;
  AddUpdatesFunc AddUpdates_;
  DenseBatchIncOpLogFunc DenseBatchIncOpLog_;
};

}  // namespace petuum
