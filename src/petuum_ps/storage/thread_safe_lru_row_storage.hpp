// Copyright (c) 2013, SailingLab
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

#ifndef PETUUM_THREAD_SAFE_LRU_ROW_STORAGE
#define PETUUM_THREAD_SAFE_LRU_ROW_STORAGE

#include "petuum_ps/include/configs.hpp"
#include "petuum_ps/storage/row_storage.hpp"
#include "petuum_ps/storage/lru_row_storage.hpp"
#include <boost/thread.hpp>

namespace petuum {

// This is the thread-safe extension of LRURowStorage. The implementation is
// naive: it locks a global mutex in every operation. Please see LRURowStorage
// for more detailed comments of each function.
template<template<typename> class ROW, typename V>
class ThreadSafeLRURowStorage : public LRURowStorage<ROW, V> {
  public:
    ThreadSafeLRURowStorage();

    explicit ThreadSafeLRURowStorage(int capacity);

    explicit ThreadSafeLRURowStorage(const StorageConfig& storage_config);

    ThreadSafeLRURowStorage(const ThreadSafeLRURowStorage& other);

    ThreadSafeLRURowStorage(int capacity, int active_list_size);

    ThreadSafeLRURowStorage(int capacity, int active_list_size,
        double num_row_access_to_active);

    virtual int Put(int32_t row_id, int32_t col_id, const V& val);

    virtual int PutRow(int32_t row_id, const ROW<V>& row_vals);

    virtual bool HasRow(int32_t row_id) const;

    virtual int Get(int32_t row_id, int32_t col_id, V* val);

    virtual int GetRow(int32_t row_id, ROW<V>* row_data, int32_t row_iter = 0);

    virtual int Inc(int32_t row_id, int32_t col_id, const V& delta);

    virtual void Erase(int32_t row_id);
  
  private:
    // Global mutex that every operation locks.
    // Comment(wdai): Use mutable so const functions can lock it as well.
    mutable boost::mutex mutex_;
};

// =============== ThreadSafeLRURowStorage Implementation ===============

template<template<typename> class ROW, typename V>
ThreadSafeLRURowStorage<ROW, V>::ThreadSafeLRURowStorage()
  : LRURowStorage<ROW, V>() { }

template<template<typename> class ROW, typename V>
ThreadSafeLRURowStorage<ROW, V>::ThreadSafeLRURowStorage(int capacity)
  : LRURowStorage<ROW, V>(capacity) { }

template<template<typename> class ROW, typename V>
ThreadSafeLRURowStorage<ROW, V>::ThreadSafeLRURowStorage(
    const StorageConfig& storage_config)
  : LRURowStorage<ROW, V>(storage_config) { }

template<template<typename> class ROW, typename V>
ThreadSafeLRURowStorage<ROW, V>::ThreadSafeLRURowStorage(
    const ThreadSafeLRURowStorage& other)
  : LRURowStorage<ROW, V>(other) { }

template<template<typename> class ROW, typename V>
ThreadSafeLRURowStorage<ROW, V>::ThreadSafeLRURowStorage(int capacity,
    int active_list_size) : LRURowStorage<ROW, V>(capacity,
      active_list_size) { }

template<template<typename> class ROW, typename V>
ThreadSafeLRURowStorage<ROW, V>::ThreadSafeLRURowStorage(int capacity,
    int active_list_size, double num_row_access_to_active)
  : LRURowStorage<ROW, V>(capacity, active_list_size,
      num_row_access_to_active) { }

template<template<typename> class ROW, typename V>
int ThreadSafeLRURowStorage<ROW, V>::Put(int32_t row_id, int32_t col_id,
    const V& val) {
  boost::lock_guard<boost::mutex> guard(mutex_);
  return LRURowStorage<ROW, V>::Put(row_id, col_id, val);
}

template<template<typename> class ROW, typename V>
int ThreadSafeLRURowStorage<ROW, V>::PutRow(int32_t row_id,
    const ROW<V>& row_vals) {
  boost::lock_guard<boost::mutex> guard(mutex_);
  return LRURowStorage<ROW, V>::PutRow(row_id, row_vals);
}

template<template<typename> class ROW, typename V>
bool ThreadSafeLRURowStorage<ROW, V>::HasRow(int32_t row_id) const {
  boost::lock_guard<boost::mutex> guard(mutex_);
  return LRURowStorage<ROW, V>::HasRow(row_id);
}

template<template<typename> class ROW, typename V>
int ThreadSafeLRURowStorage<ROW, V>::Get(int32_t row_id, int32_t col_id,
    V* val) {
  boost::lock_guard<boost::mutex> guard(mutex_);
  return LRURowStorage<ROW, V>::Get(row_id, col_id, val);
}

template<template<typename> class ROW, typename V>
int ThreadSafeLRURowStorage<ROW, V>::GetRow(int32_t row_id,
    ROW<V>* row_data, int32_t row_iter) {
  boost::lock_guard<boost::mutex> guard(mutex_);
  return LRURowStorage<ROW, V>::GetRow(row_id, row_data, row_iter);
}

template<template<typename> class ROW, typename V>
int ThreadSafeLRURowStorage<ROW, V>::Inc(int32_t row_id, int32_t col_id,
    const V& delta) {
  boost::lock_guard<boost::mutex> guard(mutex_);
  return LRURowStorage<ROW, V>::Inc(row_id, col_id, delta);
}

template<template<typename> class ROW, typename V>
void ThreadSafeLRURowStorage<ROW, V>::Erase(int32_t row_id) {
  boost::lock_guard<boost::mutex> guard(mutex_);
  return LRURowStorage<ROW, V>::Erase(row_id);
}
}  // namespace petuum

#endif  // PETUUM_THREAD_SAFE_LRU_ROW_STORAGE
