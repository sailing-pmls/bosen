// Author: jinliang
#pragma once

#include <stdint.h>
#include <map>

#include <functional>
#include <boost/noncopyable.hpp>
#include <utility>
#include <glog/logging.h>

#include <petuum_ps_common/oplog/abstract_row_oplog.hpp>

namespace petuum {
class SparseRowOpLog : public virtual AbstractRowOpLog {
public:
  SparseRowOpLog(InitUpdateFunc InitUpdate,
                 CheckZeroUpdateFunc CheckZeroUpdate,
                 size_t update_size):
      AbstractRowOpLog(update_size),
      InitUpdate_(InitUpdate),
      CheckZeroUpdate_(CheckZeroUpdate) { }

  virtual ~SparseRowOpLog() {
    auto iter = oplogs_.begin();
    for (; iter != oplogs_.end(); iter++) {
      delete[] iter->second;
    }
  }

  void Reset() {
    auto iter = oplogs_.begin();
    for (; iter != oplogs_.end(); iter++) {
      delete[] iter->second;
    }
    oplogs_.clear();
  }

  void* Find(int32_t col_id) {
    auto iter = oplogs_.find(col_id);
    if (iter == oplogs_.end()) {
      return 0;
    }
    return iter->second;
  }

  const void* FindConst(int32_t col_id) const {
    auto iter = oplogs_.find(col_id);
    if (iter == oplogs_.end()) {
      return 0;
    }
    return iter->second;
  }

  void* FindCreate(int32_t col_id) {
    auto iter = oplogs_.find(col_id);
    if (iter == oplogs_.end()) {
      uint8_t* update = new uint8_t[update_size_];
      InitUpdate_(col_id, update);
      oplogs_[col_id] = update;
      return update;
    }
    return iter->second;
  }

  // Guaranteed ordered traversal
  void* BeginIterate(int32_t *column_id) {
    iter_ = oplogs_.begin();
    if (iter_ == oplogs_.end()) {
      return 0;
    }
    *column_id = iter_->first;
    return iter_->second;
  }

  void* Next(int32_t *column_id) {
    iter_++;
    if (iter_ == oplogs_.end()) {
      return 0;
    }
    *column_id = iter_->first;
    return iter_->second;
  }

  // Guaranteed ordered traversal, in ascending order of column_id
  const void* BeginIterateConst(int32_t *column_id) const {
    const_iter_ = oplogs_.cbegin();
    if (const_iter_ == oplogs_.cend()) {
      return 0;
    }
    *column_id = const_iter_->first;
    return const_iter_->second;
  }

  const void* NextConst(int32_t *column_id) const {
    const_iter_++;
    if (const_iter_ == oplogs_.cend()) {
      return 0;
    }
    *column_id = const_iter_->first;
    return const_iter_->second;
  }

  size_t GetSize() const {
    return oplogs_.size();
  }

  size_t ClearZerosAndGetNoneZeroSize() {
    auto oplog_iter = oplogs_.begin();
    while (oplog_iter != oplogs_.end()) {
      if (CheckZeroUpdate_(oplog_iter->second)) {
        delete[] oplog_iter->second;
        auto orig_oplog_iter = oplog_iter;
        ++oplog_iter;
        oplogs_.erase(orig_oplog_iter);
      } else {
        ++oplog_iter;
      }
    }
    return oplogs_.size();
  }

  size_t GetSparseSerializedSize() {
    size_t num_updates = oplogs_.size();
    return sizeof(int32_t) + sizeof(int32_t)*num_updates
        + update_size_*num_updates;
  }

  size_t GetDenseSerializedSize() {
    LOG(FATAL) << "Sparse OpLog does not support dense serialize";
    return 0;
  }

  // Serization format:
  // 1) number of updates in that row
  // 2) total size for column ids
  // 3) total size for update array
  size_t SerializeSparse(void *mem) {
    size_t num_oplogs = oplogs_.size();
    int32_t *mem_num_updates = reinterpret_cast<int32_t*>(mem);
    *mem_num_updates = num_oplogs;

    uint8_t *mem_index = reinterpret_cast<uint8_t*>(mem) + sizeof(int32_t);
    uint8_t *mem_oplogs = mem_index + num_oplogs*sizeof(int32_t);

    for (auto oplog_iter = oplogs_.cbegin(); oplog_iter != oplogs_.cend();
         ++oplog_iter) {
      *(reinterpret_cast<int32_t*>(mem_index)) = oplog_iter->first;
      memcpy(mem_oplogs, oplog_iter->second, update_size_);
      mem_index += sizeof(int32_t);
      mem_oplogs += update_size_;
    }
    return GetSparseSerializedSize();
  }

  size_t SerializeDense(void *mem) {
    LOG(FATAL) << "Sparse OpLog does not support dense serialize";
    return GetSparseSerializedSize();
  }

  const void *ParseDenseSerializedOpLog(
      const void *mem, int32_t *num_updates,
      size_t *serialized_size) const {
    LOG(FATAL) << "Sparse OpLog does not support dense serialize";
    return mem;
  }

  void OverwriteWithDenseUpdate(const void *updates, int32_t index_st,
                                int32_t num_updates) {
    const uint8_t *updates_uint8 = reinterpret_cast<const uint8_t*>(updates);
    for (int i = 0; i < num_updates; ++i) {
      int32_t col_id = i + index_st;
      uint8_t *update;
      auto oplog_iter = oplogs_.find(col_id);
      if (oplog_iter == oplogs_.end()) {
        update = new uint8_t[AbstractRowOpLog::update_size_];
        oplogs_.insert(std::make_pair(col_id, update));
      } else {
        update = oplog_iter->second;
      }
      memcpy(update, updates_uint8
             + i*AbstractRowOpLog::update_size_,
             AbstractRowOpLog::update_size_);
    }
  }
protected:
  std::map<int32_t, uint8_t*> oplogs_;
  const InitUpdateFunc InitUpdate_;
  const CheckZeroUpdateFunc CheckZeroUpdate_;
  std::map<int32_t, uint8_t*>::iterator iter_;
  mutable std::map<int32_t, uint8_t*>::const_iterator const_iter_;
};
}
