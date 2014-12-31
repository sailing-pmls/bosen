// author: jinliang

#pragma once

#include <mutex>
#include <vector>
#include <string.h>
#include <assert.h>
#include <boost/noncopyable.hpp>
#include <type_traits>
#include <cmath>

#include <petuum_ps_common/util/lock.hpp>
#include <petuum_ps_common/storage/numeric_container_row.hpp>
#include <ml/feature/dense_feature.hpp>

namespace petuum {

// V is an arithmetic type. V is the data type and also the update type.
// V needs to be POD.
template<typename V>
class DenseRow : public NumericContainerRow<V>, boost::noncopyable {
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

  void ResetRowData(const void *data, size_t num_bytes);

  void GetWriteLock();

  void ReleaseWriteLock();

  void ApplyInc(int32_t column_id, const void *update);

  void ApplyBatchInc(const int32_t *column_ids,
    const void* update_batch, int32_t num_updates);

  void ApplyIncUnsafe(int32_t column_id, const void *update);

  void ApplyBatchIncUnsafe(const int32_t *column_ids,
    const void* update_batch, int32_t num_updates);

  double ApplyIncGetImportance(int32_t column_id, const void *update);

  double ApplyBatchIncGetImportance(const int32_t *column_ids,
    const void* update_batch, int32_t num_updates);

  double ApplyIncUnsafeGetImportance(int32_t column_id, const void *update);

  double ApplyBatchIncUnsafeGetImportance(const int32_t *column_ids,
    const void* update_batch, int32_t num_updates);

  double ApplyDenseBatchIncGetImportance(
      const void* update_batch, int32_t index_st, int32_t num_updates);

  void ApplyDenseBatchInc(
      const void* update_batch, int32_t index_st, int32_t num_updates);

  double ApplyDenseBatchIncUnsafeGetImportance(
      const void* update_batch, int32_t index_st, int32_t num_updates);

  void ApplyDenseBatchIncUnsafe(
      const void* update_batch, int32_t index_st, int32_t num_updates);

  // Thread-safe.
  V operator [](int32_t column_id) const;
  int32_t get_capacity();

  // Bulk read. Thread-safe.
  void CopyToVector(std::vector<V> *to) const;

  void CopyToDenseFeature(ml::DenseFeature<V>* to) const;

  static_assert(std::is_pod<V>::value, "V must be POD");
private:
  mutable std::mutex mtx_;
  std::vector<V> data_;
  int32_t capacity_;
};

template<typename V>
DenseRow<V>::DenseRow() { }

template<typename V>
DenseRow<V>::~DenseRow() { }

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
  std::unique_lock<std::mutex> lock(mtx_);
  DenseRow<V> *new_row = new DenseRow<V>();
  new_row->Init(capacity_);
  memcpy(new_row->data_.data(), data_.data(), capacity_*sizeof(V));
  //VLOG(0) << "Cloned, capacity_ = " << new_row->capacity_;
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
  return num_bytes;
}

template<typename V>
bool DenseRow<V>::Deserialize(const void *data, size_t num_bytes) {
  int32_t vec_size = num_bytes/sizeof(V);
  capacity_ = vec_size;
  data_.resize(vec_size);
  memcpy(data_.data(), data, num_bytes);
  return true;
}

template<typename V>
void DenseRow<V>::ResetRowData(const void *data, size_t num_bytes) {
  int32_t vec_size = num_bytes/sizeof(V);
  CHECK_EQ(capacity_, vec_size);
  memcpy(data_.data(), data, num_bytes);
}

template<typename V>
void DenseRow<V>::GetWriteLock() {
  mtx_.lock();
}

template<typename V>
void DenseRow<V>::ReleaseWriteLock() {
  mtx_.unlock();
}

template<typename V>
void DenseRow<V>::ApplyInc(int32_t column_id, const void *update) {
  std::unique_lock<std::mutex> lock(mtx_);
  ApplyIncUnsafe(column_id, update);
}

template<typename V>
void DenseRow<V>::ApplyBatchInc(const int32_t *column_ids,
    const void* update_batch, int32_t num_updates) {
  std::unique_lock<std::mutex> lock(mtx_);
  ApplyBatchIncUnsafe(column_ids, update_batch, num_updates);
}

template<typename V>
void DenseRow<V>::ApplyIncUnsafe(int32_t column_id, const void *update) {
  data_[column_id] += *(reinterpret_cast<const V*>(update));
}

template<typename V>
void DenseRow<V>::ApplyBatchIncUnsafe(const int32_t *column_ids,
  const void *update_batch, int32_t num_updates) {
  const V *update_array = reinterpret_cast<const V*>(update_batch);
  int i;
  for (i = 0; i < num_updates; ++i) {
    data_[column_ids[i]] += update_array[i];
  }
}

template<typename V>
double DenseRow<V>::ApplyIncGetImportance(int32_t column_id, const void *update) {
  std::unique_lock<std::mutex> lock(mtx_);
  return ApplyIncUnsafeGetImportance(column_id, update);
}

template<typename V>
double DenseRow<V>::ApplyBatchIncGetImportance(const int32_t *column_ids,
    const void* update_batch, int32_t num_updates) {
  std::unique_lock<std::mutex> lock(mtx_);
  return ApplyBatchIncUnsafeGetImportance(column_ids, update_batch,
                                          num_updates);
}

template<typename V>
double DenseRow<V>::ApplyIncUnsafeGetImportance(int32_t column_id,
                                                const void *update) {
  V type_update = *(reinterpret_cast<const V*>(update));
  double importance = (double(data_[column_id]) == 0) ? double(type_update)
                      : double(type_update) / double(data_[column_id]);
  data_[column_id] += type_update;
  return std::abs(importance);
}

template<typename V>
double DenseRow<V>::ApplyBatchIncUnsafeGetImportance(const int32_t *column_ids,
  const void *update_batch, int32_t num_updates) {
  const V *update_array = reinterpret_cast<const V*>(update_batch);
  int i;
  double accum_importance = 0;
  for (i = 0; i < num_updates; ++i) {
    double importance
        = (double(data_[column_ids[i]]) == 0) ? double(update_array[i])
        : double(update_array[i]) / double(data_[column_ids[i]]);
    data_[column_ids[i]] += update_array[i];

    accum_importance += std::abs(importance);
  }
  return accum_importance;
}

template<typename V>
double DenseRow<V>::ApplyDenseBatchIncUnsafeGetImportance(
    const void* update_batch, int32_t index_st, int32_t num_updates) {
  const V *update_array = reinterpret_cast<const V*>(update_batch);
  int i;
  double accum_importance = 0;
  for (i = 0; i < num_updates; ++i) {
    int col_id = i + index_st;
    double importance
        = (double(data_[col_id]) == 0) ? double(update_array[i])
        : double(update_array[i]) / double(data_[col_id]);
    data_[col_id] += update_array[i];

    accum_importance += std::abs(importance);
  }
  return accum_importance;
}

template<typename V>
void DenseRow<V>::ApplyDenseBatchIncUnsafe(
    const void* update_batch, int32_t index_st, int32_t num_updates) {
  const V *update_array = reinterpret_cast<const V*>(update_batch);
  int i;
  for (i = 0; i < num_updates; ++i) {
    int col_id = i + index_st;
    data_[col_id] += update_array[i];
  }
}

template<typename V>
double DenseRow<V>::ApplyDenseBatchIncGetImportance(
    const void* update_batch, int32_t index_st, int32_t num_updates) {
  std::unique_lock<std::mutex> lock(mtx_);

  return ApplyDenseBatchIncUnsafeGetImportance(
      update_batch, index_st, num_updates);
}

template<typename V>
void DenseRow<V>::ApplyDenseBatchInc(
    const void* update_batch, int32_t index_st, int32_t num_updates) {
  std::unique_lock<std::mutex> lock(mtx_);
  return ApplyDenseBatchIncUnsafe(update_batch, index_st, num_updates);
}

template<typename V>
V DenseRow<V>::operator [](int32_t column_id) const {
  std::unique_lock<std::mutex> lock(mtx_);
  V v = data_[column_id];
  return v;
}

template<typename V>
int32_t DenseRow<V>::get_capacity(){
  return capacity_;
}

template<typename V>
void DenseRow<V>::CopyToVector(std::vector<V> *to) const {
  std::unique_lock<std::mutex> lock(mtx_);
  //CHECK_EQ(to->size(), data_.size());
  to->resize(data_.size());
  std::copy(data_.begin(), data_.end(), to->begin());
}

template<typename V>
void DenseRow<V>::CopyToDenseFeature(ml::DenseFeature<V>* to) const {
  std::unique_lock<std::mutex> lock(mtx_);
  to->Init(data_);
}

}
