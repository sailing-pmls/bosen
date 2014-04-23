// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.02.02

#pragma once

#include "petuum_ps/include/abstract_row.hpp"
#include "petuum_ps/util/lock.hpp"
#include <boost/thread.hpp>
#include <map>
#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>
#include <glog/logging.h>

namespace petuum {

// Multi-threaded sparse row, support unbounded # of columns.  There is never
// non-zero entry in the map storage, as ApplyInc / ApplyBatchInc
// removes the zero-writes.
template<typename V>
class SparseRow : public AbstractRow, boost::noncopyable {
public:

  SparseRow(){};
  ~SparseRow(){};

  // Entry-wide read by acquiring read lock. We recommend using iterators to
  // traverse the map instead of operator[], as each [] need to acquire the
  // read lock.
  const V& operator[](int32_t col_id) const;

  // Number of (non-zero) entries in the underlying map.
  int32_t num_entries() const;

public:  // Iterator
  // const_iterator lets you do this:
  //
  //  SparseRow<int> row;
  //  ... fill in some entries ...
  //  for (SparseRow<int>::const_iterator it = row.cbegin(); !it.is_end();
  //    ++it) {
  //    int key = it->first;
  //    int val = it->second;
  //    int same_value = *it;
  //  }
  //
  // Notice the is_end() in for loop. You can't use the == operator. Also, we
  // don't need iterator as writing to the row is only done by the system
  // internally, not user.
  class const_iterator {
  public:
    typedef typename std::map<int32_t, V>::const_iterator iter_t;
    // The dereference operator returns a V type copied from the key-value
    // pair under the iterator. It fails if the iterator is already at the
    // end of the map (ie., has exhausted all nonzero entries).
    V operator*();

    // The arrow dereference operator returns a pointer to the underlying map
    // iterator.
    iter_t operator->();

    // The prefix increment operator moves the iterator forwards. It fails
    // if the iterator is already at the end of the map (ie., has exhausted
    // all nonzero entries).
    const_iterator* operator++();

    // The postfix increment operator behaves identically to the prefix
    // increment operator.
    const_iterator* operator++(int);

    // The prefix decrement operator moves the iterator backward. It fails
    // if the iterator is already at the begining of the map (ie., the first
    // nonzero entry).
    const_iterator* operator--();

    // The postfix decrement operator behaves identically to the prefix
    // decrement operator.
    const_iterator* operator--(int);

    bool is_begin();

    // Use it.is_end() as the exit condition in for loop. We do not provide
    // comparison operator. That is, can't do (it != row.cend()).
    bool is_end();

  private:
    // const_iterator holds shared_lock on the associated SortedVector
    // throughout iterator lifetime.
    boost::shared_lock<SharedMutex> read_lock_;

    //  Only let SparseRow to construct const_iterator.
    const_iterator(const SparseRow<V>& row, bool is_end);
    friend class SparseRow<V>;

    const std::map<int32_t, V>* row_data_;
    // Use map's iterator.
    iter_t it_;
  };

public:  // iterator functions.
  const_iterator cbegin() const;

  const_iterator cend() const;

public:  // AbstractRow implementation
  void Init(int32_t capacity) { }

  AbstractRow *Clone() const;

  size_t get_update_size() const {
    return sizeof(V);
  }

  size_t SerializedSize() const;

  size_t Serialize(void* bytes) const;

  bool Deserialize(const void* data, size_t num_bytes);

  // We recommend using ApplyBatchInc() as each ApplyInc acquires write
  // lock.
  void ApplyInc(int32_t column_id, const void *update);

  void ApplyBatchInc(const int32_t *column_ids,
    const void *update_batch, int32_t num_updates);

  void ApplyIncUnsafe(int32_t column_id, const void *update);

  void ApplyBatchIncUnsafe(const int32_t *column_ids,
    const void* update_batch, int32_t num_updates);

  void AddUpdates(int32_t column_id, void* update1,
    const void *update2) const;

  void SubtractUpdates(int32_t column_id, void *update1,
    const void *update2) const;

  void InitUpdate(int32_t column_id, void* zero) const;

private:
  friend class const_iterator;

  // col_id --> entry value. Entries in row_data_ are ensured to be
  // non-zero at the write time, not read time.
  std::map<int32_t, V> row_data_;

  mutable SharedMutex rw_mutex_;

  // zero for type V.
  static const V zero_;
};

// ================= Implementation =================

template<typename V>
const V SparseRow<V>::zero_ = V(0);

template<typename V>
const V& SparseRow<V>::operator[](int32_t col_id) const {
  boost::shared_lock<SharedMutex> read_lock(rw_mutex_);
  auto it = row_data_.find(col_id);   // map::find() is thread-safe.
  if (it == row_data_.cend()) {
    return zero_;
  }
  return it->second;
}

template<typename V>
int32_t SparseRow<V>::num_entries() const {
  boost::shared_lock<SharedMutex> read_lock(rw_mutex_);
  return row_data_.size();
}

// ======== const_iterator Implementation ========

template<typename V>
SparseRow<V>::const_iterator::const_iterator(const SparseRow<V>& row,
    bool is_end) : read_lock_(row.rw_mutex_), row_data_(&(row.row_data_)) {
  if (is_end) {
    it_ = row_data_->cend();
  } else {
    it_ = row_data_->cbegin();
  }
}

template<typename V>
V SparseRow<V>::const_iterator::operator*() {
  CHECK(!is_end());
  return it_->second;
}

template<typename V>
typename SparseRow<V>::const_iterator::iter_t
SparseRow<V>::const_iterator::operator->() {
  CHECK(!is_end());
  return it_;
}

template<typename V>
typename SparseRow<V>::const_iterator*
SparseRow<V>::const_iterator::operator++() {
  CHECK(!is_end());
  it_++;
  return this;
}

template<typename V>
typename SparseRow<V>::const_iterator*
SparseRow<V>::const_iterator::operator++(int unused) {
  CHECK(!is_end());
  it_++;
  return this;
}

template<typename V>
typename SparseRow<V>::const_iterator*
SparseRow<V>::const_iterator::operator--() {
  CHECK(!is_begin());
  it_--;
  return this;
}

template<typename V>
typename SparseRow<V>::const_iterator*
SparseRow<V>::const_iterator::operator--(int unused) {
  CHECK(!is_begin());
  it_--;
  return this;
}

template<typename V>
bool SparseRow<V>::const_iterator::is_begin() {
  return (it_ == row_data_->cbegin());
}

template<typename V>
bool SparseRow<V>::const_iterator::is_end() {
  return (it_ == row_data_->cend());
}

template<typename V>
typename SparseRow<V>::const_iterator SparseRow<V>::cbegin() const {
  return const_iterator(*this, false);
}

template<typename V>
typename SparseRow<V>::const_iterator SparseRow<V>::cend() const {
  return const_iterator(*this, true);
}

// ======== AbstractRow Implementation ========

template<typename V>
AbstractRow *SparseRow<V>::Clone() const {
  std::unique_lock<SharedMutex> read_lock(rw_mutex_);
  SparseRow<V> new_row = new SparseRow<V>();
  new_row->row_data_ = row_data_;
  return static_cast<AbstractRow*>(new_row);
}

template<typename V>
size_t SparseRow<V>::SerializedSize() const {
  VLOG(2) << "SerializedSize = " <<
    row_data_.size() * (sizeof(int32_t) + sizeof(V));
  return row_data_.size() * (sizeof(int32_t) + sizeof(V));
}

template<typename V>
size_t SparseRow<V>::Serialize(void* bytes) const {
  void* data_ptr = bytes;
  for (const std::pair<int32_t, V>& entry : row_data_) {
    int32_t* col_ptr = reinterpret_cast<int32_t*>(data_ptr);
    *col_ptr = entry.first;
    ++col_ptr;
    V* val_ptr = reinterpret_cast<V*>(col_ptr);
    *val_ptr = entry.second;
    data_ptr = reinterpret_cast<void*>(++val_ptr);
  }
  return SerializedSize();
}

template<typename V>
bool SparseRow<V>::Deserialize(const void* data, size_t num_bytes) {
  row_data_.clear();

  int32_t num_bytes_per_entry = (sizeof(int32_t) + sizeof(V));
  CHECK_EQ(0, num_bytes % num_bytes_per_entry) << "num_bytes = " << num_bytes;

  int32_t num_entries = num_bytes / num_bytes_per_entry;

  const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(data);
  for (int i = 0; i < num_entries; ++i) {
    const int32_t* col_ptr = reinterpret_cast<const int32_t*>(data_ptr);
    int32_t col_id = *col_ptr;
    VLOG(2) << "col_id = " << col_id;
    ++col_ptr;
    const V* val_ptr = reinterpret_cast<const V*>(col_ptr);
    V val = *val_ptr;
    data_ptr = reinterpret_cast<const uint8_t*>(++val_ptr);
    row_data_[col_id] = val;
  }

  return true;
}

template<typename V>
void SparseRow<V>::ApplyInc(int32_t column_id, const void *update) {
  std::unique_lock<SharedMutex> write_lock(rw_mutex_);
  ApplyIncUnsafe(column_id, update);
}

template<typename V>
void SparseRow<V>::ApplyBatchInc(const int32_t *column_ids,
  const void *update_batch, int32_t num_updates) {
  std::unique_lock<SharedMutex> write_lock(rw_mutex_);
  ApplyBatchIncUnsafe(column_ids, update_batch, num_updates);
}

template<typename V>
void SparseRow<V>::ApplyIncUnsafe(int32_t column_id, const void *update) {
  const V* typed_update = reinterpret_cast<const V*>(update);
  row_data_[column_id] += *typed_update;
  if (row_data_[column_id] == zero_) {
    // remove 0 entry.
    row_data_.erase(column_id);
  }
}

template<typename V>
void SparseRow<V>::ApplyBatchIncUnsafe(const int32_t *column_ids,
    const void* update_batch, int32_t num_updates) {
  const V* typed_updates = reinterpret_cast<const V*>(update_batch);
  for (int32_t i = 0; i < num_updates; ++i) {
    int32_t col_id = column_ids[i];
    row_data_[col_id] += typed_updates[i];
    if (row_data_[col_id] == zero_) {
      // remove 0 entry.
      row_data_.erase(col_id);
    }
  }
}

template<typename V>
void SparseRow<V>::AddUpdates(int32_t column_id, void* update1,
  const void* update2) const {
  // Ignore column_id
  V* typed_update1 = reinterpret_cast<V*>(update1);
  const V* typed_update2 = reinterpret_cast<const V*>(update2);
  *typed_update1 += *typed_update2;
}

template<typename V>
void SparseRow<V>::SubtractUpdates(int32_t column_id, void* update1,
  const void* update2) const {
  // Ignore column_id
  V* typed_update1 = reinterpret_cast<V*>(update1);
  const V* typed_update2 = reinterpret_cast<const V*>(update2);
  *typed_update1 -= *typed_update2;
}

template<typename V>
void SparseRow<V>::InitUpdate(int32_t column_id, void* zero) const {
  V* typed_zero = reinterpret_cast<V*>(zero);
  *typed_zero = zero_;
}

}  // namespace petuum
