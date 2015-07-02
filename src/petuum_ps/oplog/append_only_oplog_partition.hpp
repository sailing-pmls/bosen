// Author: jinliang

#pragma once

#include <petuum_ps/oplog/abstract_oplog.hpp>
#include <petuum_ps_common/util/mpmc_queue.hpp>
#include <boost/thread/tss.hpp>
#include <petuum_ps_common/oplog/abstract_append_only_buffer.hpp>
#include <petuum_ps_common/include/configs.hpp>
#include <petuum_ps_common/oplog/buffer_manager.hpp>

namespace petuum {
class AppendOnlyOpLogPartition : public AbstractOpLog {
public:
  AppendOnlyOpLogPartition(size_t buffer_capacity, const AbstractRow *sample_row,
                           AppendOnlyOpLogType append_only_oplog_type,
                           size_t dense_row_capacity,
                           size_t per_thread_buff_pool_size);
  ~AppendOnlyOpLogPartition();

  void RegisterThread();
  void DeregisterThread();
  void FlushOpLog();

  // exclusive access
  int32_t Inc(int32_t row_id, int32_t column_id, const void *delta);
  int32_t BatchInc(int32_t row_id, const int32_t *column_ids,
                const void *deltas, int32_t num_updates);
  int32_t DenseBatchInc(int32_t row_id, const void *updates,
                     int32_t index_st, int32_t num_updates);

  // Guaranteed exclusive accesses to the same row id.
  bool FindOpLog(int32_t row_id, OpLogAccessor *oplog_accessor);
  // return true if a new row oplog is created
  bool FindInsertOpLog(int32_t row_id, OpLogAccessor *oplog_accessor);
  // oplog_accessor aquires the lock on the row whether or not the
  // row oplog exists.
  bool FindAndLock(int32_t row_id, OpLogAccessor *oplog_accessor);

  // Not mutual exclusive but is less expensive than FindOpLog above as it does
  // not use any lock.
  AbstractRowOpLog *FindOpLog(int32_t row_id);
  AbstractRowOpLog *FindInsertOpLog(int32_t row_id);

  // Mutual exclusive accesses
  bool GetEraseOpLog(int32_t row_id, AbstractRowOpLog **row_oplog_ptr);
  bool GetEraseOpLogIf(int32_t row_id, GetOpLogTestFunc test,
                       void *test_args, AbstractRowOpLog **row_oplog_ptr);
  bool GetInvalidateOpLogMeta(int32_t row_id, RowOpLogMeta *row_oplog_meta);

  AbstractAppendOnlyBuffer *GetAppendOnlyBuffer(int32_t comm_channel_idx);

  void PutBackBuffer(int32_t comm_channel_idx, AbstractAppendOnlyBuffer* buff);
private:
  AbstractAppendOnlyBuffer *CreateAppendOnlyBuffer();

  const size_t buffer_capacity_;
  const size_t update_size_;
  const AbstractRow *sample_row_;
  const AppendOnlyOpLogType append_only_oplog_type_;
  const size_t dense_row_capacity_;
  const size_t per_thread_pool_size_;

  boost::thread_specific_ptr<AbstractAppendOnlyBuffer> append_only_buff_;
  MPMCQueue<AbstractAppendOnlyBuffer*> shared_queue_;

  OpLogBufferManager oplog_buffer_mgr_;
};

}   // namespace petuum
