// Author: jinliang

#pragma once

#include <stdint.h>
#include <libcuckoo/cuckoohash_map.hh>
#include <boost/unordered_map.hpp>

#include "petuum_ps/util/lock.hpp"
#include "petuum_ps/util/striped_lock.hpp"
#include "petuum_ps/thread/context.hpp"
#include "petuum_ps/include/abstract_row.hpp"
#include "petuum_ps/oplog/row_oplog.hpp"

namespace petuum {
class OpLogAccessor {
public:
  OpLogAccessor():
    row_oplog_(0) { }

  ~OpLogAccessor() { }

  Unlocker<std::mutex> *get_unlock_ptr() {
    return &unlocker_;
  }

  void set_row_oplog(RowOpLog *row_oplog) {
    row_oplog_ = row_oplog;
  }

  void* Find(int32_t col_id) {
    return row_oplog_->Find(col_id);
  }

  void* FindCreate(int32_t col_id) {
    return row_oplog_->FindCreate(col_id);
  }

  void* BeginIterate(int32_t *column_id) {
    return row_oplog_->BeginIterate(column_id);
  }

  void* Next(int32_t *column_id) {
    return row_oplog_->Next(column_id);
  }

private:
  Unlocker<std::mutex> unlocker_;
  RowOpLog *row_oplog_;
};

class OpLogPartition : boost::noncopyable {
public:
  OpLogPartition();
  OpLogPartition(int32_t capacity, const AbstractRow *sample_row,
                 int32_t table_id);
  ~OpLogPartition();

  // exclusive access
  void Inc(int32_t row_id, int32_t column_id, const void *delta);
  void BatchInc(int32_t row_id, const int32_t *column_ids,
    const void *deltas, int32_t num_updates);

  // Guaranteed exclusive accesses to the same row id.
  bool FindOpLog(int32_t row_id, OpLogAccessor *oplog_accessor);
  void FindInsertOpLog(int32_t row_id, OpLogAccessor *oplog_accessor);

  // Not mutual exclusive but is less expensive than FIndOpLog above as it does
  // not use any lock.
  RowOpLog *FindOpLog(int32_t row_id);
  RowOpLog *FindInsertOpLog(int32_t row_id);

  typedef bool (*GetOpLogTestFunc)(RowOpLog *, void *arg);
  bool GetEraseOpLog(int32_t row_id, RowOpLog **row_oplog_ptr);
  bool GetEraseOpLogIf(int32_t row_id, GetOpLogTestFunc test,
                            void *test_args, RowOpLog **row_oplog_ptr);

private:
  size_t update_size_;
  StripedLock<int32_t> locks_;
  cuckoohash_map<int32_t,  RowOpLog*> oplog_map_;
  const AbstractRow *sample_row_;
  int32_t table_id_;
};

}   // namespace petuum
