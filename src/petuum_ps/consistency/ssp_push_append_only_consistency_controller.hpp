#pragma once

#include <petuum_ps/consistency/ssp_push_consistency_controller.hpp>

namespace petuum {

class SSPPushAppendOnlyConsistencyController :
      public SSPPushConsistencyController {
public:
  SSPPushAppendOnlyConsistencyController(
      const TableInfo& info,
      int32_t table_id,
      AbstractProcessStorage& process_storage,
      AbstractOpLog& oplog,
      const AbstractRow* sample_row,
      boost::thread_specific_ptr<ThreadTable> &thread_cache,
      TableOpLogIndex &oplog_index,
      int32_t row_oplog_type);

  virtual void Inc(int32_t row_id, int32_t column_id, const void* delta);

  virtual void BatchInc(int32_t row_id, const int32_t* column_ids,
                        const void* updates, int32_t num_updates);

  virtual void DenseBatchInc(int32_t row_id, const void *updates,
                             int32_t index_st, int32_t num_updates);

  virtual void ThreadInc(int32_t row_id, int32_t column_id, const void* delta);

  // Increment column_ids.size() entries of a row. deltas points to an array.
  virtual void ThreadBatchInc(int32_t row_id, const int32_t* column_ids,
			      const void* updates, int32_t num_updates);

  virtual void ThreadDenseBatchInc(
      int32_t row_id, const void *updates, int32_t index_st,
      int32_t num_updates);

  virtual void FlushThreadCache();
  virtual void Clock();
};

}  // namespace petuum
