// author: jinliang

#pragma once

#include <vector>
#include <glog/logging.h>

#include <petuum_ps_common/storage/abstract_store.hpp>
#include <petuum_ps_common/storage/abstract_store_iterator.hpp>
namespace petuum {
// V is an arithmetic type. V is the data type and also the update type.
// V needs to be POD.
// When Init(), memory is zero-ed out.
template<typename V>
class VectorStore : public AbstractStore<V> {
public:
  VectorStore();
  ~VectorStore();

  VectorStore(const VectorStore<V> &other);
  VectorStore<V> & operator = (const VectorStore<V> &other);

  // When init(), the memory is zeroed out.
  void Init(size_t capacity);
  size_t SerializedSize() const;
  size_t Serialize(void *bytes) const;
  void Deserialize(const void *data, size_t num_bytes);
  void ResetData(const void *data, size_t num_bytes);

  V Get (int32_t col_id) const;
  void Inc(int32_t col_id, V delta);

  V *GetPtr(int32_t col_id);
  const V *GetConstPtr(int32_t col_id) const;

  size_t get_capacity() const;

  const void CopyToVector(void *to) const;
  const void CopyToMem(void *to) const;

  const void *GetDataPtr() const;

private:
  std::vector<V> data_;
};

template<typename V>
VectorStore<V>::VectorStore() { }

template<typename V>
VectorStore<V>::~VectorStore() { }

template<typename V>
VectorStore<V>::VectorStore(const VectorStore<V> &other):
    data_(other.data_) { }

template<typename V>
VectorStore<V> & VectorStore<V>::operator = (const VectorStore<V> &other) {
  data_ = other.data_;
  return *this;
}

template<typename V>
void VectorStore<V>::Init(size_t capacity) {
  data_.resize(capacity);
  memset(data_.data(), 0, capacity * sizeof(V));
}

template<typename V>
size_t VectorStore<V>::SerializedSize() const {
  return data_.size() * sizeof(V);
}

template<typename V>
size_t VectorStore<V>::Serialize(void *bytes) const {
  size_t num_bytes = data_.size()*sizeof(V);
  memcpy(bytes, data_.data(), num_bytes);
  return num_bytes;
}

template<typename V>
void VectorStore<V>::Deserialize(const void *data, size_t num_bytes) {
  size_t num_entries = num_bytes / sizeof(V);
  data_.resize(num_entries);
  memcpy(data_.data(), data, num_bytes);
}

template<typename V>
void VectorStore<V>::ResetData(const void *data, size_t num_bytes) {
  memcpy(data_.data(), data, num_bytes);
}

template<typename V>
V VectorStore<V>::Get (int32_t col_id) const {
  V v = data_[col_id];
  return v;
}

template<typename V>
void VectorStore<V>::Inc(int32_t col_id, V delta) {
  data_[col_id] += delta;
}

template<typename V>
V* VectorStore<V>::GetPtr(int32_t col_id) {
  return data_.data() + col_id;
}

template<typename V>
const V* VectorStore<V>::GetConstPtr(int32_t col_id) const {
  return data_.data() + col_id;
}

template<typename V>
size_t VectorStore<V>::get_capacity() const {
  return data_.size();
}

template<typename V>
const void VectorStore<V>::CopyToVector(void *to) const {
  auto &vec = *(reinterpret_cast<std::vector<V>*>(to));
  memcpy(vec.data(), data_.data(), data_.size()*sizeof(V));
}

template<typename V>
const void VectorStore<V>::CopyToMem(void *to) const {
  memcpy(to, data_.data(), data_.size()*sizeof(V));
}

template<typename V>
const void *VectorStore<V>::GetDataPtr() const {
  return &data_;
}

}
