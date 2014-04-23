// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.03.23

#pragma once

#include "petuum_ps/include/abstract_row.hpp"
#include "petuum_ps/util/lock.hpp"
#include "petuum_ps/util/stats.hpp"
#include <boost/thread.hpp>
#include <cstdint>
#include <vector>
#include <utility>
#include <glog/logging.h>
#include <algorithm>

namespace petuum {

// A row entry. Row is sorted on Entry.first (int32_t).
template<typename V>
using Entry = std::pair<int32_t, V>;


// SortedVectorMapRow stores pairs of (int32_t, V) in an array, sorted on
// int32_t (the weight) in descending order. Like map, it supports unbounded
// number of items through dynamic memory allocation.
template<typename V>
class SortedVectorMapRow : public AbstractRow {
public:  // AbstractRow override.
  SortedVectorMapRow();

  // Number of (non-zero) entries in the underlying vector.
  int32_t num_entries() const;

private:  // private functions
  // Return vector index of associated with column_id, or -1 if not found.
  int32_t FindIndex(int32_t column_id) const;

  // Move vector_idx forward (or backward) to maintain descending order.
  void OneSidedBubbleSort(int32_t vector_idx, bool forward);

  // Allocate new memory with new_capacity, which must be at least
  // num_entries_. block_aligned sets capacity_ to the smallest multiples of
  // K_BLOCK_SIZE_.
  void SetAndAllocateCapacity(int32_t new_capacity, bool block_aligned = true);

  // Remove vector_idx and shift the rest forward.
  void RemoveOneEntryAndCompact(int32_t vector_idx);

public:  // AbstractRow implementation.
  // Reserve memory in entries_ to be able to store 'capacity' entries.
  void Init(int32_t capacity);

  AbstractRow *Clone() const;

  size_t get_update_size() const;

  size_t SerializedSize() const;

  size_t Serialize(void *bytes) const;

  bool Deserialize(const void* data, size_t num_bytes);

  void ApplyInc(int32_t column_id, const void *update);

  void ApplyBatchInc(const int32_t *column_ids,
      const void* updates, int32_t num_updates);

  void ApplyIncUnsafe(int32_t column_id, const void *update);

  void ApplyBatchIncUnsafe(const int32_t *column_ids, const void* updates,
      int32_t num_updates);

  void AddUpdates(int32_t column_id, void* update1, const void* update2) const;

  void SubtractUpdates(int32_t column_id, void *update1,
      const void* update2) const;

  void InitUpdate(int32_t column_id, void* zero) const;

public:  // Iterator
  // const_iterator lets you do this:
  //
  //  SortedVectorMapRow<int> row;
  //  ... fill in some entries ...
  //  for (SortedVectorMapRow<int>::const_iterator it = row.cbegin();
  //    !it.is_end(); ++it) {
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
    typedef Entry<V>* iter_t;
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
    // const_iterator holds shared_lock on the associated SortedVectorMapRow
    // throughout iterator lifetime.
    boost::shared_lock<SharedMutex> read_lock_;

    //  Only let SortedVectorMapRow to construct const_iterator. Does not take
    //  ownership of row.
    const_iterator(const SortedVectorMapRow<V>& row, bool is_end);
    friend class SortedVectorMapRow<V>;

    // entries_ and num_entries_ are obtained from row.
    Entry<V>* entries_;
    int32_t num_entries_;

    // The index of the entry to return upon dereferencing;
    // (curr_ == row->num_entries_) implies the end.
    int32_t curr_;
  };

public:  // iterator functions.
  const_iterator cbegin() const;

  const_iterator cend() const;

public:   // static constants
  // Size to increase or shrink entries_; in # of entries. The larger it is
  // the less memory efficient, but less memory allocation.
  static const int32_t K_BLOCK_SIZE_;

private:
  // Array of sorted entries.
  std::unique_ptr<Entry<V>[]> entries_;

  // # of entries in entries_.
  int32_t num_entries_;

  // # of entries entries_ can hold.
  int32_t capacity_;

  mutable SharedMutex rw_mutex_;
};

// ================ Implementation =================

template<typename V>
const int32_t SortedVectorMapRow<V>::K_BLOCK_SIZE_ = 10;

template<typename V>
SortedVectorMapRow<V>::SortedVectorMapRow() :
  num_entries_(0), capacity_(0) { }

template<typename V>
int32_t SortedVectorMapRow<V>::num_entries() const {
  boost::shared_lock<SharedMutex> read_lock(rw_mutex_);
  return num_entries_;
}

// ================ Private Methods =================

template<typename V>
int32_t SortedVectorMapRow<V>::FindIndex(int32_t column_id) const {
  for (int i = 0; i < num_entries_; ++i) {
    if (entries_[i].first == column_id) {
      return i;
    }
  }
  return -1;
}

template<typename V>
void SortedVectorMapRow<V>::OneSidedBubbleSort(int32_t vector_idx,
    bool forward) {
  if (forward) {
    // The correct index for vector_idx; initially assume vector_idx doesn't
    // need to be moved.
    int32_t new_idx = vector_idx;
    V& val = entries_[vector_idx].second;
    for (int i = vector_idx + 1; i < num_entries_; ++i) {
      if (val < entries_[i].second) {
        new_idx = i;
      } else {
        break;
      }
    }
    if (new_idx > vector_idx) {
      // Do the move.
      Entry<V> tmp = entries_[vector_idx];
      memmove(entries_.get() + vector_idx, entries_.get() + vector_idx + 1,
          (new_idx - vector_idx) * sizeof(Entry<V>));
      entries_[new_idx] = tmp;
    }
  } else {
    // Move backward
    int32_t new_idx = vector_idx;
    V& val = entries_[vector_idx].second;
    for (int i = vector_idx - 1; i >= 0; --i) {
      if (val > entries_[i].second) {
        new_idx = i;
      } else {
        break;
      }
    }
    if (new_idx < vector_idx) {
      // Do the move.
      Entry<V> tmp = entries_[vector_idx];
      memmove(entries_.get() + new_idx + 1, entries_.get() + new_idx,
          (vector_idx - new_idx) * sizeof(Entry<V>));
      entries_[new_idx] = tmp;
    }
  }
}

template<typename V>
void SortedVectorMapRow<V>::SetAndAllocateCapacity(int32_t new_capacity,
    bool block_aligned) {
  CHECK_GE(new_capacity, num_entries_);
  int32_t remainder = new_capacity % K_BLOCK_SIZE_;
  if (block_aligned && remainder != 0) {
    new_capacity += K_BLOCK_SIZE_ - remainder;
  }
  //VLOG(0) << "Expanding capacity to " << new_capacity;
  capacity_ = new_capacity;
  Entry<V>* new_entries = new Entry<V>[capacity_];
  memcpy(new_entries, entries_.get(), num_entries_ * sizeof(Entry<V>));
  entries_.reset(new_entries);
}

template<typename V>
void SortedVectorMapRow<V>::RemoveOneEntryAndCompact(int32_t vector_idx) {
  memcpy(entries_.get() + vector_idx, entries_.get() + vector_idx + 1,
      (num_entries_ - vector_idx - 1) * sizeof(Entry<V>));
  --num_entries_;
  // Compact criterion.
  if (capacity_ - num_entries_ >= 2 * K_BLOCK_SIZE_) {
    // block_aligned sets capacity is to multiples of K_BLOCK_SIZE_.
    SetAndAllocateCapacity(num_entries_, true);
  }
}

// ======== AbstractRow Implementation ========

template<typename V>
void SortedVectorMapRow<V>::Init(int32_t capacity) {
  capacity_ = capacity;
  entries_.reset(new Entry<V>[capacity_]);
}


template<typename V>
AbstractRow *SortedVectorMapRow<V>::Clone() const {
  std::unique_lock<SharedMutex> read_lock(rw_mutex_);
  SortedVectorMapRow<V> *new_row = new SortedVectorMapRow<V>();
  new_row->Init(capacity_);
  new_row->capacity_ = capacity_;
  memcpy(new_row->entries_.get(), entries_.get(), num_entries_*sizeof(Entry<V>));
  new_row->num_entries_ = num_entries_;

  return static_cast<AbstractRow*>(new_row);
}

template<typename V>
size_t SortedVectorMapRow<V>::get_update_size() const {
  return sizeof(V);
}

template<typename V>
size_t SortedVectorMapRow<V>::SerializedSize() const {
  return num_entries_ * sizeof(Entry<V>);
}

template<typename V>
size_t SortedVectorMapRow<V>::Serialize(void *bytes) const {
  size_t num_bytes = SerializedSize();
  memcpy(bytes, entries_.get(), num_bytes);
  return num_bytes;
}

template<typename V>
bool SortedVectorMapRow<V>::Deserialize(const void* data, size_t num_bytes) {
  int32_t num_bytes_per_entry = sizeof(Entry<V>);
  CHECK_EQ(0, num_bytes % num_bytes_per_entry);
  // Cannot set num_entries_, which would cause SetAndAllocateCapacity to
  // attempt to read entries_.
  int32_t num_entries = num_bytes / num_bytes_per_entry;
  SetAndAllocateCapacity(num_entries, true);
  num_entries_ = num_entries;
  memcpy(entries_.get(), data, num_bytes);
  return true;
}

template<typename V>
void SortedVectorMapRow<V>::ApplyInc(int32_t column_id, const void *update) {
  std::unique_lock<SharedMutex> write_lock(rw_mutex_);
  ApplyIncUnsafe(column_id, update);
}

template<typename V>
void SortedVectorMapRow<V>::ApplyBatchInc(const int32_t *column_ids,
    const void* updates, int32_t num_updates) {
  std::unique_lock<SharedMutex> write_lock(rw_mutex_);
  ApplyBatchIncUnsafe(column_ids, updates, num_updates);
}

template<typename V>
void SortedVectorMapRow<V>::ApplyIncUnsafe(int32_t column_id,
    const void *update) {
  // Go through the array and find column_id
  int32_t vector_idx = FindIndex(column_id);
  if (vector_idx != -1) {
    V typed_update = *(reinterpret_cast<const V*>(update));
    entries_[vector_idx].second += typed_update;

    // Remove vector_idx if vector_idx becomes 0.
    if (entries_[vector_idx].second == V(0)) {
      RemoveOneEntryAndCompact(vector_idx);
      return;
    }

    // Move vector_idx to maintain sorted order.
    bool forward = typed_update <= V(0);
    OneSidedBubbleSort(vector_idx, forward);
    return;
  }
  // Add a new entry.
  if (num_entries_ == capacity_) {
    SetAndAllocateCapacity(capacity_ + K_BLOCK_SIZE_, true);
  }

  entries_[num_entries_++] = std::make_pair(column_id,
      *(reinterpret_cast<const V*>(update)));
  // Move new entry to maintain sorted order. Always move backward.
  OneSidedBubbleSort(num_entries_ - 1, false);
}


// TODO(wdai): It's the same as entry-wise Inc, except the sorting is applied
// after all applying all inc. Consider calling ApplyIncUnsafe when
// num_updates is small.
template<typename V>
void SortedVectorMapRow<V>::ApplyBatchIncUnsafe(const int32_t *column_ids,
    const void* updates, int32_t num_updates) {

  TIMER_BEGIN(0, SORTED_VECTOR_MAP_BATCH_INC_UNSAFE);

  const V* typed_updates = reinterpret_cast<const V*>(updates);

  // Use ApplyInc individually on each column_id.
  for (int i = 0; i < num_updates; ++i) {
    int32_t vector_idx = FindIndex(column_ids[i]);
    if (vector_idx != -1) {
      entries_[vector_idx].second += typed_updates[i];

      // Remove vector_idx if vector_idx becomes 0.
      if (entries_[vector_idx].second == V(0)) {
        RemoveOneEntryAndCompact(vector_idx);
      }
      continue;
    }
    // Add a new entry.
    if (num_entries_ == capacity_) {
      SetAndAllocateCapacity(capacity_ + K_BLOCK_SIZE_, true);
    }

    entries_[num_entries_++] = std::make_pair(column_ids[i], typed_updates[i]);
  }

  // Global sort.
  std::sort(entries_.get(), entries_.get() + num_entries_,
      [](const std::pair<int32_t, V>& i, const std::pair<int32_t, V>&j) {
      return i.second > j.second; });

  TIMER_END(0, SORTED_VECTOR_MAP_BATCH_INC_UNSAFE);
}

template<typename V>
void SortedVectorMapRow<V>::AddUpdates(int32_t column_id, void* update1,
    const void* update2) const {
  *(reinterpret_cast<V*>(update1)) += *(reinterpret_cast<const V*>(update2));
}

template<typename V>
void SortedVectorMapRow<V>::SubtractUpdates(int32_t column_id, void *update1,
    const void* update2) const {
  *(reinterpret_cast<V*>(update1)) -= *(reinterpret_cast<const V*>(update2));
}

template<typename V>
void SortedVectorMapRow<V>::InitUpdate(int32_t column_id, void* zero) const {
  *(reinterpret_cast<V*>(zero)) = V(0);
}

// ======== const_iterator Implementation ========

template<typename V>
SortedVectorMapRow<V>::const_iterator::const_iterator(
    const SortedVectorMapRow<V>& row, bool is_end) :
  read_lock_(row.rw_mutex_), entries_(row.entries_.get()),
  num_entries_(row.num_entries_) {
    if (is_end) {
      curr_ = num_entries_;
    } else {
      curr_ = 0;
    }
  }

template<typename V>
V SortedVectorMapRow<V>::const_iterator::operator*() {
  CHECK(!is_end());
  return entries_[curr_]->second;
}

template<typename V>
typename SortedVectorMapRow<V>::const_iterator::iter_t
SortedVectorMapRow<V>::const_iterator::operator->() {
  CHECK(!is_end());
  return &(entries_[curr_]);
}

template<typename V>
typename SortedVectorMapRow<V>::const_iterator*
SortedVectorMapRow<V>::const_iterator::operator++() {
  CHECK(!is_end());
  curr_++;
  return this;
}

template<typename V>
typename SortedVectorMapRow<V>::const_iterator*
SortedVectorMapRow<V>::const_iterator::operator++(int unused) {
  CHECK(!is_end());
  curr_++;
  return this;
}

template<typename V>
typename SortedVectorMapRow<V>::const_iterator*
SortedVectorMapRow<V>::const_iterator::operator--() {
  CHECK(!is_begin());
  curr_--;
  return this;
}

template<typename V>
typename SortedVectorMapRow<V>::const_iterator*
SortedVectorMapRow<V>::const_iterator::operator--(int unused) {
  CHECK(!is_begin());
  curr_--;
  return this;
}

template<typename V>
bool SortedVectorMapRow<V>::const_iterator::is_begin() {
  return (curr_ == 0);
}

template<typename V>
bool SortedVectorMapRow<V>::const_iterator::is_end() {
  return (curr_ == num_entries_);
}

template<typename V>
typename SortedVectorMapRow<V>::const_iterator
SortedVectorMapRow<V>::cbegin() const {
  return const_iterator(*this, false);
}

template<typename V>
typename SortedVectorMapRow<V>::const_iterator
SortedVectorMapRow<V>::cend() const {
  return const_iterator(*this, true);
}

}  // namespace petuum
