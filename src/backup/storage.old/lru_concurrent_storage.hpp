#ifndef LRU_CONCURRENT_STORAGE
#define LRU_CONCURRENT_STORAGE

#include "petuum_ps/include/configs.hpp"
#include "petuum_ps/storage/row_storage.hpp"
#include "petuum_ps/storage/lru_eviction_logic.hpp"
#include "petuum_ps/storage/lru_buffer.hpp"

#include <boost/thread.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <tbb/concurrent_hash_map.h>
#include <glog/logging.h>

namespace petuum {

// ConcurrentRowStorage only allows a constant number of rows to be in the
// storage. User has to make sure that there's room in the storage before
// adding a new row, or the storage will randomly evict a row. See RowStorage
// for detail on the semantics of each function.
template<template<typename> class ROW, typename V>
class ConcurrentRowStorage : public RowStorage<ROW, V> {
  public:
    ConcurrentRowStorage();

    explicit ConcurrentRowStorage(const StorageConfig& storage_config);

    explicit ConcurrentRowStorage(int capacity);

    ConcurrentRowStorage(const ConcurrentRowStorage& other);

    virtual int get_num_rows() const;

    virtual int Put(int32_t row_id, int32_t col_id, const V& val);

    virtual int PutRow(int32_t row_id, const ROW<V>& row_vals);

    virtual bool HasRow(int32_t row_id) const;

    virtual int Get(int32_t row_id, int32_t col_id, V* val);

    virtual int Get(int32_t row_id, int32_t col_id, V* val, int32_t *row_iter);

    virtual int GetRow(int32_t row_id, ROW<V>* row_data, int32_t row_iter = 0);

    virtual int Inc(int32_t row_id, int32_t col_id, const V& delta);

    virtual void Erase(int32_t row_id);

  private:
    // Comment(wdai): Use tbb::unordered_hash_map to allow concurrent
    // insertion and erasure. This way we don't need to have global
    // boost::shared_mutex to lock for all operations.
    typedef boost::tuple<ROW<V>, typename EvictionLogic::list_iter_t> element_t;
    typedef tbb::concurrent_hash_map<int32_t, element_t>
      storage_type;

    // Underlying storage is a hash map.
    storage_type storage_;

    // this is the buffer used to lazy update for the eviction logic
    MultiBuffer* buffers_;



    // Only allow one thread to do PutRow() and Erase() at the same time.
    //
    // Comment(wdai): We can only have one thread inserting new row, or it's
    // possible that when we have (capacity_ - 1) rows, and two threads both
    // insert a new row, and they both detect there are (capacity_ + 1) rows
    // and both delete a row, and we are left with (capacity_ - 1) rows,
    // whereas we'd only want to delte 1 row and have (capacity_) rows.
    //
    // Comment(wdai): We still need tbb::concurrent_hash_map so other thread
    // can read while one thread is erasing a row.
    mutable boost::mutex insert_erase_row_mutex_;
};

// =============== ConcurrentRowStorage Implementation ===============

template<template<typename> class ROW, typename V>
ConcurrentRowStorage<ROW, V>::ConcurrentRowStorage() :
  RowStorage<ROW, V>(0)
  {
    buffers_ = new MultiBuffer(2, 10);
  }

template<template<typename> class ROW, typename V>
ConcurrentRowStorage<ROW, V>::ConcurrentRowStorage(int capacity) :
  RowStorage<ROW, V>(capacity)
  {
    buffers_ = new MultiBuffer(2, 10);
  }

template<template<typename> class ROW, typename V>
ConcurrentRowStorage<ROW, V>::ConcurrentRowStorage(
    const StorageConfig& storage_config) :
  RowStorage<ROW, V>(storage_config)
  {
    buffers_ = new MultiBuffer(2, 10);
  }

template<template<typename> class ROW, typename V>
ConcurrentRowStorage<ROW, V>::ConcurrentRowStorage(
    const ConcurrentRowStorage& other) : RowStorage<ROW, V>(other),
  storage_(other.storage_)
  {
    buffers_ = new MultiBuffer(2, 10);
  }

template<template<typename> class ROW, typename V>
int ConcurrentRowStorage<ROW, V>::get_num_rows() const {
  // We could get a wrong number of rows if get_num_rows() is called during
  // PutRow() or Erase().
  boost::unique_lock<boost::mutex> check_size_lock(insert_erase_row_mutex_);
  return storage_.size();
}

template<template<typename> class ROW, typename V>
int ConcurrentRowStorage<ROW, V>::Put(int32_t row_id, int32_t col_id,
    const V& val) {
  // accessor acquires a write lock on the key-value pair.
  typename storage_type::accessor row_acc;
  if (storage_.find(row_acc, row_id)) {
    // Update the row.
    ROW<V>& row_data = boost::get<0>(row_acc->second);
    row_data[col_id] = val;
    buffers_->add_to_buffer(boost::get<1>(row_acc->second));
    return 1;
  }
  return 0;  // row_id not found. Return 0 for no-op.
}

template<template<typename> class ROW, typename V>
int ConcurrentRowStorage<ROW, V>::Inc(int32_t row_id, int32_t col_id,
    const V& delta) {
  // accessor acquires a write lock on the key-value pair.
  typename storage_type::accessor row_acc;
  if (storage_.find(row_acc, row_id)) {
    // Update the row.
    ROW<V>& row_data = boost::get<0>(row_acc->second);
    row_data[col_id] += delta;
    buffers_->add_to_buffer(boost::get<1>(row_acc->second));
    return 1;
  }
  return 0;  // row_id not found. Return 0 for no-op.
}

template<template<typename> class ROW, typename V>
int ConcurrentRowStorage<ROW, V>::PutRow(int32_t row_id,
    const ROW<V>& row_vals) {
  // Look for the row and obtain a write lock on that row.
  typename storage_type::accessor row_acc;
  if (storage_.find(row_acc, row_id)) {
    // We found it.
    boost::get<0>(row_acc->second) = row_vals;
    buffers_->add_to_buffer(boost::get<1>(row_acc->second));
    return 0;   // replacing existing row.
  }
  // Cannot find it. Insert it now and lock insert_erase_row_mutex_.
  boost::unique_lock<boost::mutex> insert_lock(insert_erase_row_mutex_);
  while (static_cast<int>(storage_.size()) >= this->capacity_) {
    // TODO: this is not atomic and not thread safe
    int32_t id = buffers_->evict_drain_buffer();
    if (!storage_.erase(id)) {
      LOG(WARNING) << "Other thread has erased row " << row_id
        << " between the Erase call and mutex lock.";
    }
  }
  // TODO: Is this serialized?
  if (!storage_.insert(row_acc, row_id))
  {
    LOG(WARNING) << "same as replacing";
  }
  boost::get<0>(row_acc->second) = row_vals;
  boost::get<1>(row_acc->second) = buffers_->put(row_id);
  return 1;   // insert a new row.
}

template<template<typename> class ROW, typename V>
bool ConcurrentRowStorage<ROW, V>::HasRow(int32_t row_id) const {
  // const_accessor acquires a read lock.
  typename storage_type::const_accessor row_acc;
  return storage_.find(row_acc, row_id);
}

template<template<typename> class ROW, typename V>
int ConcurrentRowStorage<ROW, V>::Get(int32_t row_id, int32_t col_id, V* val,
    int32_t *row_iter) {
  // const_accessor acquires a read lock on the row.
  typename storage_type::const_accessor row_acc;
  if (!storage_.find(row_acc, row_id)) {
    return 0;
  }

  const ROW<V>& row_data = boost::get<0>(row_acc->second);
  *val = row_data[col_id];
  *row_iter = row_data.get_iteration();
  buffers_->add_to_buffer(boost::get<1>(row_acc->second));
  return 1;
}

template<template<typename> class ROW, typename V>
int ConcurrentRowStorage<ROW, V>::Get(int32_t row_id, int32_t col_id, V* val) {
  // const_accessor acquires a read lock on the row.
  typename storage_type::const_accessor row_acc;
  if (!storage_.find(row_acc, row_id)) {
    return 0;
  }

  const ROW<V>& row_data = boost::get<0>(row_acc->second);
  *val = row_data[col_id];
  buffers_->add_to_buffer(boost::get<1>(row_acc->second));
  return 1;
}

template<template<typename> class ROW, typename V>
int ConcurrentRowStorage<ROW, V>::GetRow(int32_t row_id, ROW<V>* row_data,
    int32_t row_iter) {
  // const_accessor acquires a read lock on the row.
  typename storage_type::const_accessor row_acc;
  if (!storage_.find(row_acc, row_id)) {
    VLOG(0) << "row not found";
    return 0;
  }
  const ROW<V>& row = boost::get<0>(row_acc->second);
  if (row.get_iteration() >= row_iter) {
    *row_data = row;
    buffers_->add_to_buffer(boost::get<1>(row_acc->second));
    return 1;
  }
  VLOG(0) << "not fresh enough";
  // Found it but too stale.
  return 0;
}

template<template<typename> class ROW, typename V>
void ConcurrentRowStorage<ROW, V>::Erase(int32_t row_id) {
  boost::unique_lock<boost::mutex> erase_lock(insert_erase_row_mutex_);
  if (!storage_.erase(row_id)) {
    LOG(WARNING) << "Other thread has erased row " << row_id
      << " between the Erase call and mutex lock.";
  }
}

}  // namespace petuum

#endif  // LRU_TBB_WRAPPER
