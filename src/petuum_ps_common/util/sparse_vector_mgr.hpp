#pragma once

#include <petuum_ps_common/util/sparse_vector.hpp>

namespace petuum {
class SparseVectorMgr : boost::noncopyable {
public:
  SparseVectorMgr(size_t capacity, size_t value_size):
      capacity_(capacity),
      value_size_(value_size),
      sparse_vector_(new SparseVector(capacity, value_size)) { }

  ~SparseVectorMgr() { }

  size_t get_capacity() const {
    return capacity_;
  }

  size_t get_size() const {
    return sparse_vector_->get_size();
  }

  uint8_t* GetValPtr(int32_t key) {
    uint8_t *val_ptr = sparse_vector_->GetValPtr(key);
    if (val_ptr == 0) {
      capacity_ *= 2;
      SparseVector *new_sparse_vector = new SparseVector(capacity_, value_size_);
      new_sparse_vector->Copy(*(sparse_vector_));
      sparse_vector_.reset(new_sparse_vector);
      val_ptr = sparse_vector_->GetValPtr(key);
    }
    return val_ptr;
  }

  const uint8_t* GetValPtrConst(int32_t key) const {
    const uint8_t *val_ptr = sparse_vector_->GetValPtrConst(key);
    return val_ptr;
  }

  void ResetVal() {
    sparse_vector_->Reset();
  }

  void ResetSizeAndShrink() {
    if (sparse_vector_->get_capacity() >= 2
        && sparse_vector_->get_size() < sparse_vector_->get_capacity()/2) {
      SparseVector *new_sparse_vector = new SparseVector(
          capacity_, value_size_);
      sparse_vector_.reset(new_sparse_vector);
    } else {
      ResetVal();
    }
  }

  uint8_t* GetByIdx(int32_t index, int32_t *key) {
    return sparse_vector_->GetByIdx(index, key);
  }

  const uint8_t* GetByIdxConst(int32_t index, int32_t *key) const {
    return sparse_vector_->GetByIdxConst(index, key);
  }

  uint8_t* get_data_ptr() {
    return sparse_vector_->get_data_ptr();
  }

private:
  size_t capacity_;
  const size_t value_size_;
  std::unique_ptr<SparseVector> sparse_vector_;
};

}
