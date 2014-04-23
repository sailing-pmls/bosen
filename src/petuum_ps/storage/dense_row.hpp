// author: jinliang

#pragma once

#include <mutex>
#include <vector>
#include <string.h>
#include <assert.h>
#include <boost/noncopyable.hpp>

#include "petuum_ps/util/lock.hpp"
#include "petuum_ps/include/abstract_row.hpp"

namespace petuum {

// V is an arithmetic type. V is the data type and also the update type.
template<typename V>
class DenseRow : public AbstractRow, boost::noncopyable {
public:
  DenseRow();
  ~DenseRow();
  void Init(int32_t capacity);

  size_t get_update_size() const {
    return sizeof(V);
  }

  AbstractRow *Clone() const;
  size_t SerializedSize() const;
  size_t Serialize(void *bytes) const;
  bool Deserialize(const void *data, size_t num_bytes);
  void ApplyInc(int32_t column_id, const void *update);

  void ApplyBatchInc(const int32_t *column_ids,
    const void* update_batch, int32_t num_updates);

  void ApplyIncUnsafe(int32_t column_id, const void *update);
  void ApplyBatchIncUnsafe(const int32_t *column_ids,
    const void* update_batch, int32_t num_updates);

  void AddUpdates(int32_t column_id, void *update1,
    const void *update2) const;

  void SubtractUpdates(int32_t column_id, void *update1,
    const void *update2) const;

  void InitUpdate(int32_t column_id, void *update) const;

  V operator [](int32_t column_id) const;
  int32_t get_capacity();
  void CopyToVector(std::vector<V> *to) const;
private:
  mutable SharedMutex smtx_;
  std::vector<V> data_;
  int32_t capacity_;
};

template<typename V>
DenseRow<V>::DenseRow(){}

template<typename V>
DenseRow<V>::~DenseRow(){}

template<typename V>
void DenseRow<V>::Init(int32_t capacity) {
  data_.resize(capacity);
  int i;
  for(i = 0; i < capacity; ++i){
    data_[i] = V(0);
  }
  capacity_ = capacity;
}

template<typename V>
AbstractRow *DenseRow<V>::Clone() const {
  boost::shared_lock<SharedMutex> read_lock(smtx_);
  DenseRow<V> *new_row = new DenseRow<V>();
  new_row->Init(capacity_);
  memcpy(new_row->data_.data(), data_.data(), capacity_*sizeof(V));
  VLOG(0) << "Cloned, capacity_ = " << new_row->capacity_;
  return static_cast<AbstractRow*>(new_row);
}

template<typename V>
size_t DenseRow<V>::SerializedSize() const {
  return data_.size()*sizeof(V);
}

template<typename V>
size_t DenseRow<V>::Serialize(void *bytes) const {
  size_t num_bytes = data_.size()*sizeof(V);
  memcpy(bytes, data_.data(), num_bytes);
  //VLOG(0) << "Serialize DenseRow, size = " << data_.size();
  //VLOG(0) << "data_[200] = " << data_[200];
  return num_bytes;
}

template<typename V>
bool DenseRow<V>::Deserialize(const void *data, size_t num_bytes) {
  VLOG(0) << "Deserialize called";
  int32_t vec_size = num_bytes/sizeof(V);
  capacity_ = vec_size;
  data_.resize(vec_size);
  memcpy(data_.data(), data, num_bytes);
  return true;
}

template<typename V>
void DenseRow<V>::ApplyInc(int32_t column_id, const void *update){
  assert(column_id < (int32_t) data_.size());
  std::unique_lock<SharedMutex> write_lock(smtx_);
  data_[column_id] += *(reinterpret_cast<const V*>(update));
}

template<typename V>
void DenseRow<V>::ApplyBatchInc(const int32_t *column_ids,
    const void* update_batch, int32_t num_updates){
  const V *update_array = reinterpret_cast<const V*>(update_batch);

  std::unique_lock<SharedMutex> write_lock(smtx_);
  int i;
  for(i = 0; i < num_updates; ++i){
    data_[column_ids[i]] += update_array[i];
  }
}

template<typename V>
void DenseRow<V>::ApplyIncUnsafe(int32_t column_id, const void *update){
  assert(column_id < (int32_t) data_.size());
  data_[column_id] += *(reinterpret_cast<const V*>(update));
}

template<typename V>
void DenseRow<V>::ApplyBatchIncUnsafe(const int32_t *column_ids,
  const void *update_batch, int32_t num_updates){
  const V *update_array = reinterpret_cast<const V*>(update_batch);
  int i;
  for(i = 0; i < num_updates; ++i){
    data_[column_ids[i]] += update_array[i];
    //VLOG(0) << "Increment " << column_ids[i] << " by " << update_array[i];
  }
}


template<typename V>
void DenseRow<V>::AddUpdates(int32_t column_id, void *update1,
  const void *update2) const {
  *(reinterpret_cast<V*>(update1)) += *(reinterpret_cast<const V*>(update2));
}

template<typename V>
void DenseRow<V>::SubtractUpdates(int32_t column_id, void *update1,
  const void *update2) const {
  *(reinterpret_cast<V*>(update1)) -= *(reinterpret_cast<const V*>(update2));
}

template<typename V>
void DenseRow<V>::InitUpdate(int32_t column_id, void *update) const {
  *(reinterpret_cast<V*>(update)) = V(0);
}

template<typename V>
V DenseRow<V>::operator [](int32_t column_id) const {
  boost::shared_lock<SharedMutex> read_lock(smtx_);
  VLOG(0) << "my capacity = " << capacity_;
  V v = data_[column_id];
  return v;
}

template<typename V>
int32_t DenseRow<V>::get_capacity(){
  return capacity_;
}

template<typename V>
void DenseRow<V>::CopyToVector(std::vector<V> *to) const {
  to->resize(data_.size());
  memcpy(to->data(), data_.data(), data_.size()*sizeof(V));
}

}
