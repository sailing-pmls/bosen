// Copyright (c) 2014, Sailing Lab
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the <ORGANIZATION> nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
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
void DenseRow<V>::Init(int32_t capacity){
  data_.resize(capacity);
  int i;
  for(i = 0; i < capacity; ++i){
    data_[i] = V(0);
  }
  capacity_ = capacity;
}

template<typename V>
size_t DenseRow<V>::SerializedSize() const {
  return data_.size()*sizeof(V);
}

template<typename V>
size_t DenseRow<V>::Serialize(void *bytes) const {
  size_t num_bytes = data_.size()*sizeof(V);
  memcpy(bytes, data_.data(), num_bytes);
  VLOG(0) << "Serialize DenseRow, size = " << data_.size();
  VLOG(0) << "data_[3] = " << data_[3];
  return num_bytes;
}

template<typename V>
bool DenseRow<V>::Deserialize(const void *data, size_t num_bytes) {
  int32_t vec_size = num_bytes/sizeof(V);
  const V *data_array = reinterpret_cast<const V*>(data);
  data_.resize(vec_size);
  int i;
  for(i = 0; i < vec_size; ++i){
    data_[i] = data_array[i];
  }
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
  V v = data_[column_id];
  return v;
}

template<typename V>
int32_t DenseRow<V>::get_capacity(){
  return capacity_;
}

}
