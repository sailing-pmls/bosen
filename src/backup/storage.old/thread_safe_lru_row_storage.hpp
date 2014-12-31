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
  
  virtual void SetThrStats(StatsObj *thr_stats, int32_t table_id);

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
template<template<typename> class ROW, typename V>
void ThreadSafeLRURowStorage<ROW, V>::SetThrStats(StatsObj *thr_stats, 
						  int32_t table_id) {
#ifdef PETUUM_stats
  boost::lock_guard<boost::mutex> guard(mutex_);
  LRURowStorage<ROW, V>::SetThrStats(thr_stats, table_id);
#endif
}
}  // namespace petuum

#endif  // PETUUM_THREAD_SAFE_LRU_ROW_STORAGE
