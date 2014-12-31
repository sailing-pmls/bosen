#pragma once

#include <petuum_ps_common/consistency/abstract_consistency_controller.hpp>
#include <petuum_ps_common/util/vector_clock_mt.hpp>
#include <petuum_ps_sn/client/thread_table.hpp>
#include <utility>
#include <vector>
#include <cstdint>
#include <atomic>

namespace petuum {

class LocalConsistencyController : public AbstractConsistencyController {
public:
  LocalConsistencyController(const ClientTableConfig& config,
    int32_t table_id,
    ProcessStorage& process_storage,
    const AbstractRow* sample_row,
    boost::thread_specific_ptr<ThreadTableSN> &thread_cache);

  virtual void GetAsync(int32_t row_id) { }
  virtual void WaitPendingAsnycGet() { }

  // Check freshness; make request and block if too stale or row_id not found
  // in storage.
  virtual void Get(int32_t row_id, RowAccessor* row_accessor);

  // Return immediately.
  virtual void Inc(int32_t row_id, int32_t column_id, const void* delta);

  virtual void BatchInc(int32_t row_id, const int32_t* column_ids,
                        const void* updates,
			int32_t num_updates);

  virtual void ThreadGet(int32_t row_id, ThreadRowAccessor* row_accessor);

  virtual void ThreadInc(int32_t row_id, int32_t column_id, const void* delta);

  // Increment column_ids.size() entries of a row. deltas points to an array.
  virtual void ThreadBatchInc(int32_t row_id, const int32_t* column_ids,
			      const void* updates, int32_t num_updates);

  virtual void FlushThreadCache();
  virtual void Clock();

protected:
  void CheckWaitClock(int32_t staleness);
  virtual void CreateInsertRow(int32_t row_id, RowAccessor *row_accessor);

  // SSP staleness parameter.
  int32_t staleness_;

  int32_t row_type_;

  int32_t row_capacity_;

  // Locks_ guarantees exclusive accesses to rows outside of the process cache,
  // whether it's on disk or does not exist.
  StripedLock<int32_t> locks_;
  boost::thread_specific_ptr<ThreadTableSN> &thread_cache_;
};

}  // namespace petuum
