#ifndef PETUUM_SIMPLE_ROW_STORAGE
#define PETUUM_SIMPLE_ROW_STORAGE

#include "petuum_ps/include/configs.hpp"
#include "petuum_ps/storage/row_storage.hpp"

#include <boost/thread.hpp>
#include <boost/unordered_map.hpp>
#include <glog/logging.h>

namespace petuum {

// SimpleRowStorage only allows a constant number of rows to be in the
// storage. User has to make sure that there's room in the storage before
// adding a new row, or PutRow will return error code and insert nothing. See
// RowStorage for detail on the semantics of each function.
// Note that SimpleRowStorage is single threaded.
template<template<typename> class ROW, typename V>
class SimpleRowStorage : public RowStorage<ROW, V> {
  public:
    SimpleRowStorage();

    explicit SimpleRowStorage(const StorageConfig& storage_config);

    explicit SimpleRowStorage(int capacity);

    SimpleRowStorage(const SimpleRowStorage& other);

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
    // Underlying storage is a hash map.
    typedef typename boost::unordered_map<int32_t, ROW<V> > storage_t;
    storage_t storage_;
};

// =============== SimpleRowStorage Implementation ===============

template<template<typename> class ROW, typename V>
SimpleRowStorage<ROW, V>::SimpleRowStorage() :
  RowStorage<ROW, V>(0) { }

template<template<typename> class ROW, typename V>
SimpleRowStorage<ROW, V>::SimpleRowStorage(int capacity) :
  RowStorage<ROW, V>(capacity) { }

template<template<typename> class ROW, typename V>
SimpleRowStorage<ROW, V>::SimpleRowStorage(
    const StorageConfig& storage_config) :
  RowStorage<ROW, V>(storage_config) { }

template<template<typename> class ROW, typename V>
SimpleRowStorage<ROW, V>::SimpleRowStorage(
    const SimpleRowStorage& other) : RowStorage<ROW, V>(other),
  storage_(other.storage_) { }

template<template<typename> class ROW, typename V>
int SimpleRowStorage<ROW, V>::get_num_rows() const {
  return storage_.size();
}

template<template<typename> class ROW, typename V>
int SimpleRowStorage<ROW, V>::Put(int32_t row_id, int32_t col_id,
    const V& val) {
  typename storage_t::iterator it = storage_.find(row_id);
  if (it != storage_.end()) {
    // Update the row.
    ROW<V>& row_data = it->second;
    row_data[col_id] = val;
    return 1;
  }
  return 0;  // row_id not found. Return 0 for no-op.
}

template<template<typename> class ROW, typename V>
int SimpleRowStorage<ROW, V>::Inc(int32_t row_id, int32_t col_id,
    const V& delta) {
  typename storage_t::iterator it = storage_.find(row_id);
  if (it != storage_.end()) {
    // Update the row.
    ROW<V>& row_data = it->second;
    row_data[col_id] += delta;
    return 1;
  }
  return 0;  // row_id not found. Return 0 for no-op.
}

template<template<typename> class ROW, typename V>
int SimpleRowStorage<ROW, V>::PutRow(int32_t row_id,
    const ROW<V>& row_vals) {
  typename storage_t::iterator it = storage_.find(row_id);
  if (it != storage_.end()) {
    // We found it.
    it->second = row_vals;
    return 0;   // replacing existing row.
  }
  if (static_cast<int>(storage_.size()) == this->capacity_) {
    LOG(WARNING) << "Hitting storage limit (capacity = " << this->capacity_
      << "). Abandon PutRow and return -1";
    return -1;
  }

  // insert a new row.
  storage_[row_id] = row_vals;
  return 1;
}

template<template<typename> class ROW, typename V>
bool SimpleRowStorage<ROW, V>::HasRow(int32_t row_id) const {
  typename storage_t::const_iterator it = storage_.find(row_id);
  return it != storage_.end();
}

template<template<typename> class ROW, typename V>
int SimpleRowStorage<ROW, V>::Get(int32_t row_id, int32_t col_id, V* val,
    int32_t *row_iter) {
  typename storage_t::const_iterator it = storage_.find(row_id);
  if (it == storage_.end()) {
    return 0;
  }

  const ROW<V>& row_data = it->second;
  *val = row_data[col_id];
  *row_iter = row_data.get_iteration();
  return 1;
}

template<template<typename> class ROW, typename V>
int SimpleRowStorage<ROW, V>::Get(int32_t row_id, int32_t col_id, V* val) {
  // const_accessor acquires a read lock on the row.
  typename storage_t::const_iterator it = storage_.find(row_id);
  if (it == storage_.end()) {
    return 0;
  }

  const ROW<V>& row_data = it->second;
  *val = row_data[col_id];
  return 1;
}

template<template<typename> class ROW, typename V>
int SimpleRowStorage<ROW, V>::GetRow(int32_t row_id, ROW<V>* row_data,
    int32_t row_iter) {
  typename storage_t::const_iterator it = storage_.find(row_id);
  if (it == storage_.end()) {
    VLOG(0) << "row not found";
    return 0;
  }
  VLOG(0) << "row found, check iter";
  const ROW<V>& row = it->second;
  if (row.get_iteration() >= row_iter) {
    VLOG(0) << row.get_iteration() << " is fresher than row_iter "
        << row_iter;
    *row_data = row;
    return 1;
  }
  VLOG(0) << "not fresh enough";
  // Found it but too stale.
  return 0;
}

template<template<typename> class ROW, typename V>
void SimpleRowStorage<ROW, V>::Erase(int32_t row_id) {
  storage_.erase(row_id);
}

}  // namespace petuum

#endif  //  PETUUM_CONCURRENT_ROW_STORAGE
