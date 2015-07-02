// author: jinliang

#pragma once

#include <vector>
#include <glog/logging.h>

#include <petuum_ps_common/storage/abstract_store.hpp>
#include <petuum_ps_common/storage/abstract_store_iterator.hpp>
namespace petuum {
// V must be float.
// When Init(), memory is zero-ed out.
template<typename V>
class VectorStoreFloat16 : public AbstractStore<V> {
public:
  VectorStoreFloat16();
  ~VectorStoreFloat16();

  VectorStoreFloat16(const VectorStoreFloat16<V> &other);
  VectorStoreFloat16<V> & operator = (const VectorStoreFloat16<V> &other);

  // When init(), the memory is zeroed out.
  void Init(size_t capacity);
  size_t SerializedSize() const;
  size_t Serialize(void *bytes) const;
  void Deserialize(const void *data, size_t num_bytes);
  void ResetData(const void *data, size_t num_bytes);

  V Get (int32_t col_id) const;
  void Inc(int32_t col_id, V delta);

  V *GetPtr(int32_t col_id);

  size_t get_capacity() const;

  const void Copy(void *to) const;

  const void *GetDataPtr() const;

private:
  std::vector<V> data_;
};

template<typename V>
VectorStoreFloat16<V>::VectorStoreFloat16() { }

template<typename V>
VectorStoreFloat16<V>::~VectorStoreFloat16() { }

template<typename V>
VectorStoreFloat16<V>::VectorStoreFloat16(const VectorStoreFloat16<V> &other):
    data_(other.data_) { }

template<typename V>
VectorStoreFloat16<V> & VectorStoreFloat16<V>::operator = (const VectorStoreFloat16<V> &other) {
  data_ = other.data_;
  return *this;
}

template<typename V>
void VectorStoreFloat16<V>::Init(size_t capacity) {
  data_.resize(capacity);
  memset(data_.data(), 0, capacity * sizeof(V));
}

template<typename V>
size_t VectorStoreFloat16<V>::SerializedSize() const {
  return data_.size() * sizeof(uint16_t);
}

template<typename V>
size_t VectorStoreFloat16<V>::Serialize(void *bytes) const {
  uint16_t *typed_mem = reinterpret_cast<uint16_t*>(bytes);
  for (int i = 0; i < data_.size(); ++i) {
    typed_mem[i] = Float16Compressor::compress(data_[i]);
  }

  return data_.size()*sizeof(uint16_t);
}

template<typename V>
void VectorStoreFloat16<V>::Deserialize(const void *data, size_t num_bytes) {
  size_t num_entries = num_bytes / sizeof(uint16_t);
  data_.resize(num_entries);
  const uint16_t *typed_mem = reinterpret_cast<const uint16_t*>(data);
  for (int i = 0; i < data_.size(); ++i) {
    data_[i] = Float16Compressor::decompress(typed_mem[i]);
  }
}

template<typename V>
void VectorStoreFloat16<V>::ResetData(const void *data, size_t num_bytes) {
  const uint16_t *typed_mem = reinterpret_cast<const uint16_t*>(data);
  for (int i = 0; i < data_.size(); ++i) {
    data_[i] = Float16Compressor::decompress(typed_mem[i]);
  }
}

template<typename V>
V VectorStoreFloat16<V>::Get (int32_t col_id) const {
  V v = data_[col_id];
  return v;
}

template<typename V>
void VectorStoreFloat16<V>::Inc(int32_t col_id, V delta) {
  data_[col_id] += delta;
}

template<typename V>
V* VectorStoreFloat16<V>::GetPtr(int32_t col_id) {
  return data_.data() + col_id;
}

template<typename V>
size_t VectorStoreFloat16<V>::get_capacity() const {
  return data_.size();
}

template<typename V>
const void VectorStoreFloat16<V>::Copy(void *to) const {
  std::vector<V> *vec = reinterpret_cast<std::vector<V>*>(to);
  memcpy(vec->data(), data_.data(), data_.size()*sizeof(V));
}

template<typename V>
const void *VectorStoreFloat16<V>::GetDataPtr() const {
  return &data_;
}

}
