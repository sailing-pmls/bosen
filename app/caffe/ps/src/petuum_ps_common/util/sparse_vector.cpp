#include <petuum_ps_common/util/sparse_vector.hpp>

namespace petuum {

size_t SparseVector::get_capacity() const {
  return capacity_;
}

size_t SparseVector::get_size() const {
  return size_;
}

uint8_t* SparseVector::GetByIdx(int32_t index, int32_t *key) {
  *key = GetKeyByIdx(index);
  return GetValPtrByIdx(index);
}

const uint8_t* SparseVector::GetByIdxConst(int32_t index, int32_t *key) const {
  *key = GetKeyByIdxConst(index);
  return GetValPtrByIdxConst(index);
}

int32_t &SparseVector::GetKeyByIdx(int32_t index) {
  return *(reinterpret_cast<int32_t*>(
      data_.get() + index*(sizeof(int32_t) + value_size_)));
}

uint8_t* SparseVector::GetValPtrByIdx(int32_t index) {
  return data_.get() + index*(sizeof(int32_t) + value_size_) + sizeof(int32_t);
}

const int32_t &SparseVector::GetKeyByIdxConst(int32_t index) const {
  return *(reinterpret_cast<int32_t*>(
      data_.get() + index*(sizeof(int32_t) + value_size_)));
}

const uint8_t* SparseVector::GetValPtrByIdxConst(int32_t index) const {
  return data_.get() + index*(sizeof(int32_t) + value_size_) + sizeof(int32_t);
}

uint8_t* SparseVector::GetPtrByIdx(int32_t index) {
  return data_.get() + index*(sizeof(int32_t) + value_size_);
}

bool SparseVector::Search(int32_t key, int32_t st, int32_t end,
                          int32_t *found_idx) const {
  bool found = false;
  *found_idx = -1;
  while (st <= end) {
    *found_idx = (st + end)/2;
    if (GetKeyByIdxConst(*found_idx) == key) {
      found = true;
      break;
    } else if (GetKeyByIdxConst(*found_idx) > key) {
      end = *found_idx - 1;
    } else {
      st = *found_idx + 1;
    }
  }
  return found;
}

void SparseVector::InsertAtIndex(int32_t index, int32_t key) {
  memmove(GetPtrByIdx(index + 1), GetPtrByIdx(index),
          (size_ - index)*(sizeof(int32_t) + value_size_));
  GetKeyByIdx(index) = key;
  ++size_;
}

uint8_t *SparseVector::GetValPtr(int32_t key) {
  int32_t st = 0, end = size_ - 1;
  int32_t found_idx;
  bool found = Search(key, st, end, &found_idx);

  if (found)
    return GetValPtrByIdx(found_idx);
  else if (capacity_ < size_ + 1)
    return 0;
  else if (found_idx < 0) { // empty buffer
    GetKeyByIdx(0) = key;
    ++size_;
    return GetValPtrByIdx(0);
  } else if (GetKeyByIdx(found_idx) > key) {
    InsertAtIndex(found_idx, key);
    return GetValPtrByIdx(found_idx);
  } else if (GetKeyByIdx(found_idx) < key) {
    if (found_idx + 1 < size_) {
      InsertAtIndex(found_idx + 1, key);
    } else {
      GetKeyByIdx(found_idx + 1) = key;
      ++size_;
    }
    return GetValPtrByIdx(found_idx + 1);
  } else {
    LOG(FATAL) << "Impossible to reach here!";
  }
  return 0;
}

const uint8_t* SparseVector::GetValPtrConst(int32_t key) const {
  int32_t st = 0, end = size_ - 1;
  int32_t found_idx;
  bool found = Search(key, st, end, &found_idx);
  if (found)
    return GetValPtrByIdxConst(found_idx);
  else
    return 0;
}

void SparseVector::Copy(const SparseVector &src) {
  memcpy(data_.get(), src.data_.get(), src.size_*(sizeof(int32_t) + value_size_));
  size_ = src.size_;
}

void SparseVector::Compact(int32_t index) {
  memmove(GetPtrByIdx(index), GetPtrByIdx(index + 1),
          (size_ - index - 1)*(sizeof(int32_t) + value_size_));
  --size_;
}

}
