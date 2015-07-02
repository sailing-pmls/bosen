// Author: jinliang
#pragma once

#include <stdint.h>
#include <map>

#include <functional>
#include <boost/noncopyable.hpp>
#include <utility>
#include <glog/logging.h>

#include <petuum_ps_common/oplog/abstract_row_oplog.hpp>
#include <petuum_ps_common/util/sparse_vector_mgr.hpp>

namespace petuum {
const size_t kOpLogRowInitCapacity = 8;

class SparseVectorRowOpLog : public virtual AbstractRowOpLog {
public:
  SparseVectorRowOpLog(
      size_t init_capacity,
      InitUpdateFunc InitUpdate,
      CheckZeroUpdateFunc CheckZeroUpdate,
      size_t update_size):
      AbstractRowOpLog(update_size),
      oplogs_(init_capacity, update_size),
      InitUpdate_(InitUpdate),
      CheckZeroUpdate_(CheckZeroUpdate) {
    CHECK(init_capacity > 0);
    memset(oplogs_.get_data_ptr(), 0,
           (update_size + sizeof(int32_t))*oplogs_.get_capacity());
  }

  virtual ~SparseVectorRowOpLog() { }

  void Reset() {
    oplogs_.ResetSizeAndShrink();
    memset(oplogs_.get_data_ptr(), 0,
           (update_size_ + sizeof(int32_t))*oplogs_.get_capacity());
  }

  void* Find(int32_t col_id) {
    return oplogs_.GetValPtr(col_id);
  }

  const void* FindConst(int32_t col_id) const {
    return oplogs_.GetValPtrConst(col_id);
  }

  void* FindCreate(int32_t col_id) {
    return oplogs_.GetValPtr(col_id);
  }

  // Guaranteed ordered traversal
  void* BeginIterate(int32_t *column_id) {
    iter_index_ = 0;
    return oplogs_.GetByIdx(iter_index_, column_id);
  }

  void* Next(int32_t *column_id) {
    ++iter_index_;
    if (iter_index_ == oplogs_.get_size()) {
      return 0;
    }
    return oplogs_.GetByIdx(iter_index_, column_id);
  }

  // Guaranteed ordered traversal, in ascending order of column_id
  const void* BeginIterateConst(int32_t *column_id) const {
    iter_index_ = 0;
    return oplogs_.GetByIdxConst(iter_index_, column_id);
  }

  const void* NextConst(int32_t *column_id) const {
    ++iter_index_;
    if (iter_index_ == oplogs_.get_size()) {
      return 0;
    }
    return oplogs_.GetByIdxConst(iter_index_, column_id);
  }

  size_t GetSize() const {
    return oplogs_.get_size();
  }

  size_t ClearZerosAndGetNoneZeroSize() {
    int32_t col_id = 0;
    size_t num_nonzeros = 0;
    for (int32_t idx = 0; idx < oplogs_.get_size(); ++idx) {
      uint8_t *update = oplogs_.GetByIdx(idx, &col_id);
      if (!CheckZeroUpdate_(update))
        ++num_nonzeros;
    }
    num_nonzeros_ = num_nonzeros;
    return num_nonzeros;
  }

  size_t GetSparseSerializedSize() {
    return sizeof(int32_t) + sizeof(int32_t)*num_nonzeros_
        + update_size_*num_nonzeros_;
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
    int32_t col_id = 0;
    int32_t *mem_num_updates = reinterpret_cast<int32_t*>(mem);
    *mem_num_updates = num_nonzeros_;

    uint8_t *mem_index = reinterpret_cast<uint8_t*>(mem) + sizeof(int32_t);
    uint8_t *mem_oplogs = mem_index + num_nonzeros_*sizeof(int32_t);

    for (int32_t idx = 0; idx < oplogs_.get_size(); ++idx) {
      uint8_t *update = oplogs_.GetByIdx(idx, &col_id);
      if (!CheckZeroUpdate_(update))
        continue;

      *(reinterpret_cast<int32_t*>(mem_index)) = col_id;
      memcpy(mem_oplogs, update, update_size_);
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
      uint8_t *update = oplogs_.GetValPtr(col_id);

      memcpy(update, updates_uint8
             + i*AbstractRowOpLog::update_size_,
             AbstractRowOpLog::update_size_);
    }
  }
protected:
  SparseVectorMgr oplogs_;
  size_t num_nonzeros_;
  const InitUpdateFunc InitUpdate_;
  const CheckZeroUpdateFunc CheckZeroUpdate_;
  mutable int32_t iter_index_;
};

}
