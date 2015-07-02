// Author: jinliang

#pragma once

#include <stdint.h>
#include <boost/unordered_map.hpp>

#include <petuum_ps_common/util/lock.hpp>
#include <petuum_ps_common/oplog/abstract_row_oplog.hpp>
#include <petuum_ps_common/oplog/abstract_append_only_buffer.hpp>
#include <petuum_ps_common/include/abstract_row.hpp>
#include <petuum_ps/oplog/row_oplog_meta.hpp>
#include <petuum_ps/oplog/create_row_oplog.hpp>

namespace petuum {
class OpLogAccessor {
public:
  OpLogAccessor():
    row_oplog_(0) { }

  ~OpLogAccessor() { }

  Unlocker<std::mutex> *get_unlock_ptr() {
    return &unlocker_;
  }

  void set_row_oplog(AbstractRowOpLog *row_oplog) {
    row_oplog_ = row_oplog;
  }

  AbstractRowOpLog *get_row_oplog() {
    return row_oplog_;
  }

private:
  Unlocker<std::mutex> unlocker_;
  AbstractRowOpLog *row_oplog_;
};

class AbstractOpLog : boost::noncopyable {
public:
  AbstractOpLog() { }
  virtual ~AbstractOpLog() { }

  virtual void RegisterThread() = 0;
  virtual void DeregisterThread() = 0;
  virtual void FlushOpLog() = 0;

  // exclusive access
  virtual int32_t Inc(int32_t row_id, int32_t column_id, const void *delta) = 0;
  virtual int32_t BatchInc(int32_t row_id, const int32_t *column_ids,
                           const void *deltas, int32_t num_updates) = 0;

  virtual int32_t DenseBatchInc(int32_t row_id, const void *updates,
                           int32_t index_st, int32_t num_updates) = 0;

  // Guaranteed exclusive accesses to the same row id.
  virtual bool FindOpLog(int32_t row_id, OpLogAccessor *oplog_accessor) = 0;
  // return true if a new row oplog is created
  virtual bool FindInsertOpLog(int32_t row_id, OpLogAccessor *oplog_accessor) = 0;
  // oplog_accessor aquires the lock on the row whether or not the
  // row oplog exists.
  virtual bool FindAndLock(int32_t row_id, OpLogAccessor *oplog_accessor) = 0;

  // Not mutual exclusive but is less expensive than FIndOpLog above as it does
  // not use any lock.
  virtual AbstractRowOpLog *FindOpLog(int32_t row_id) = 0;
  virtual AbstractRowOpLog *FindInsertOpLog(int32_t row_id) = 0;
  virtual void PutBackBuffer(int32_t comm_channel_idx, AbstractAppendOnlyBuffer* buff) = 0;
  // Mutual exclusive accesses
  typedef bool (*GetOpLogTestFunc)(AbstractRowOpLog *, void *arg);
  virtual bool GetEraseOpLog(
      int32_t row_id, AbstractRowOpLog **row_oplog_ptr) = 0;
  virtual bool GetEraseOpLogIf(
      int32_t row_id, GetOpLogTestFunc test,
      void *test_args, AbstractRowOpLog **row_oplog_ptr) = 0;

  virtual bool GetInvalidateOpLogMeta(
      int32_t row_id, RowOpLogMeta *row_oplog_meta) = 0;

  virtual AbstractAppendOnlyBuffer *GetAppendOnlyBuffer(int32_t comm_channel_idx) = 0;
};

}   // namespace petuum
