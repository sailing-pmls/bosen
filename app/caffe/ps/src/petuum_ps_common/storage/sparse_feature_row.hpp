// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.10.16

#pragma once

#include <boost/thread.hpp>
#include <cstdint>
#include <vector>
#include <utility>
#include <glog/logging.h>
#include <algorithm>
#include <type_traits>
#include <boost/noncopyable.hpp>

#include <ml/feature/sparse_feature.hpp>
#include <petuum_ps_common/storage/numeric_container_row.hpp>
#include <petuum_ps_common/storage/entry.hpp>
#include <petuum_ps_common/util/lock.hpp>
#include <petuum_ps_common/util/stats.hpp>

namespace petuum {

// SparseFeatureRow stores pairs of (int32_t, V) in an array (int32_t for
// column_id), sorted on column index in descending order. Like map, it
// supports unbounded number of items through dynamic memory allocation.
template<typename V>
class SparseFeatureRow : private ml::SparseFeature<V>,
  public NumericContainerRow<V> {
//  public boost::noncopyable {
public:  // AbstractRow override.
  // Number of (non-zero) entries in the underlying vector.
  int32_t GetNumEntries() const;

  V operator[](int32_t column_id) const;

  // Bulk read. Thread-safe.
  void CopyToSparseFeature(ml::SparseFeature<V>* to) const;

public:  // AbstractRow implementation.
  // Does not allocate memory.
  void Init(int32_t feature_dim);

  AbstractRow *Clone() const;

  size_t get_update_size() const;

  size_t SerializedSize() const;

  size_t Serialize(void *bytes) const;

  bool Deserialize(const void* data, size_t num_bytes);

  void ResetRowData(const void *data, size_t num_bytes);

  void GetWriteLock();

  void ReleaseWriteLock();

  void ApplyInc(int32_t column_id, const void *update);

  void ApplyBatchInc(const int32_t *column_ids,
      const void* updates, int32_t num_updates);

  void ApplyIncUnsafe(int32_t column_id, const void *update);

  void ApplyBatchIncUnsafe(const int32_t *column_ids, const void* updates,
      int32_t num_updates);

  double ApplyIncGetImportance(int32_t column_id, const void *update);

  double ApplyBatchIncGetImportance(const int32_t *column_ids,
      const void* updates, int32_t num_updates);

  double ApplyIncUnsafeGetImportance(int32_t column_id, const void *update);

  double ApplyBatchIncUnsafeGetImportance(const int32_t *column_ids,
                                          const void* updates,
                                          int32_t num_updates);

  double ApplyDenseBatchIncGetImportance(
      const void* update_batch, int32_t index_st, int32_t num_updates);

  void ApplyDenseBatchInc(
      const void* update_batch, int32_t index_st, int32_t num_updates);

  double ApplyDenseBatchIncUnsafeGetImportance(
      const void* update_batch, int32_t index_st, int32_t num_updates);

  void ApplyDenseBatchIncUnsafe(
      const void* update_batch, int32_t index_st, int32_t num_updates);

public:  // Iterator
  // const_iterator lets you do this:
  //
  //  SparseFeatureRow<int> row;
  //  ... fill in some entries ...
  //  for (SparseFeatureRow<int>::const_iterator it = row.cbegin();
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
    // const_iterator holds shared_lock on the associated SparseFeatureRow
    // throughout iterator lifetime.
    boost::shared_lock<SharedMutex> read_lock_;

    //  Only let SparseFeatureRow to construct const_iterator. Does not take
    //  ownership of row.
    const_iterator(const SparseFeatureRow<V>& row, bool is_end);
    friend class SparseFeatureRow<V>;

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

private:
  mutable SharedMutex rw_mutex_;
};

// ================ Implementation =================

template<typename V>
int32_t SparseFeatureRow<V>::GetNumEntries() const {
  boost::shared_lock<SharedMutex> read_lock(rw_mutex_);
  return this->num_entries_;
}

// ================ Private Methods =================

template<typename V>
V SparseFeatureRow<V>::operator [] (int32_t column_id) const {
  boost::shared_lock<SharedMutex> read_lock(rw_mutex_);
  return ml::SparseFeature<V>::operator[](column_id);
}

template<typename V>
void SparseFeatureRow<V>::CopyToSparseFeature(ml::SparseFeature<V>* to) const {
  CHECK_NOTNULL(to);
  boost::shared_lock<SharedMutex> read_lock(rw_mutex_);
  *to = *dynamic_cast<const ml::SparseFeature<V>*>(this);
}

// ======== AbstractRow Implementation ========

template<typename V>
void SparseFeatureRow<V>::Init(int32_t feature_dim) {
  this->feature_dim_ = feature_dim;
}


template<typename V>
AbstractRow* SparseFeatureRow<V>::Clone() const {
  std::unique_lock<SharedMutex> read_lock(rw_mutex_);
  SparseFeatureRow<V>* new_row = new SparseFeatureRow<V>();
  new_row->feature_dim_ = this->feature_dim_;
  new_row->ResetCapacity(this->num_entries_);
  memcpy(new_row->entries_.get(), this->entries_.get(),
      this->num_entries_ * sizeof(Entry<V>));
  new_row->num_entries_ = this->num_entries_;

  return static_cast<AbstractRow*>(new_row);
}

template<typename V>
size_t SparseFeatureRow<V>::get_update_size() const {
  return sizeof(V);
}

template<typename V>
size_t SparseFeatureRow<V>::SerializedSize() const {
  // Add feature_dim_ at the beginning.
  return sizeof(int32_t) + this->num_entries_ * sizeof(Entry<V>);
}

template<typename V>
size_t SparseFeatureRow<V>::Serialize(void *bytes) const {
  size_t num_bytes = SerializedSize();
  int32_t* bytes_int = reinterpret_cast<int32_t*>(bytes);
  memcpy(bytes_int, &this->feature_dim_, sizeof(int32_t));
  ++bytes_int;
  memcpy(bytes_int, this->entries_.get(),
      this->num_entries_ * sizeof(Entry<V>));
  return num_bytes;
}

template<typename V>
bool SparseFeatureRow<V>::Deserialize(const void* data, size_t num_bytes) {
  int32_t num_bytes_per_entry = sizeof(Entry<V>);
  int num_bytes_data = num_bytes - sizeof(int32_t);
  CHECK_EQ(0, num_bytes_data % num_bytes_per_entry);
  int32_t num_entries = num_bytes_data / num_bytes_per_entry;
  const int32_t* data_int = reinterpret_cast<const int32_t*>(data);
  this->feature_dim_ = *data_int;
  this->num_entries_ = num_entries;
  ++data_int;
  ml::SparseFeature<V>::ResetCapacity(num_entries);
  memcpy(this->entries_.get(), data_int, num_bytes_data);
  return true;
}

template<typename V>
void SparseFeatureRow<V>::ResetRowData(const void *data, size_t num_bytes) {
  int32_t num_bytes_per_entry = sizeof(Entry<V>);
  int num_bytes_data = num_bytes - sizeof(int32_t);   // feature_dim_ is int32_t
  CHECK_EQ(0, num_bytes_data % num_bytes_per_entry);
  int32_t num_entries = num_bytes_data / num_bytes_per_entry;
  const int32_t* data_int = reinterpret_cast<const int32_t*>(data);
  this->feature_dim_ = *data_int;
  ++data_int;
  ml::SparseFeature<V>::ResetCapacity(num_entries);

  memcpy(this->entries_.get(), data_int, num_bytes_data);
  this->num_entries_ = num_entries;

  if (this->capacity_ - this->num_entries_ >= 2 * this->K_BLOCK_SIZE_) {
    // block_aligned sets capacity is to multiples of this->K_BLOCK_SIZE_.
    ml::SparseFeature<V>::ResetCapacity(this->num_entries_);
  }
}

template<typename V>
void SparseFeatureRow<V>::GetWriteLock() {
  rw_mutex_.lock();
}

template<typename V>
void SparseFeatureRow<V>::ReleaseWriteLock() {
  rw_mutex_.unlock();
}

template<typename V>
void SparseFeatureRow<V>::ApplyInc(int32_t column_id, const void *update) {
  std::unique_lock<SharedMutex> write_lock(rw_mutex_);
  ApplyIncUnsafe(column_id, update);
}

template<typename V>
void SparseFeatureRow<V>::ApplyBatchInc(const int32_t *column_ids,
    const void* updates, int32_t num_updates) {
  std::unique_lock<SharedMutex> write_lock(rw_mutex_);
  ApplyBatchIncUnsafe(column_ids, updates, num_updates);
}

template<typename V>
void SparseFeatureRow<V>::ApplyIncUnsafe(int32_t column_id,
    const void *update) {
  // Go through the array and find column_id
  int32_t vector_idx = ml::SparseFeature<V>::FindIndex(column_id);
  V typed_update = *(reinterpret_cast<const V*>(update));
  if (vector_idx != -1) {
    this->entries_[vector_idx].second += typed_update;

    // Remove vector_idx if vector_idx becomes 0.
    if (this->entries_[vector_idx].second == V(0)) {
      ml::SparseFeature<V>::RemoveOneEntryAndCompact(vector_idx);
      return;
    }
    return;
  }

  // Add a new entry.
  if (this->num_entries_ == this->capacity_) {
    ml::SparseFeature<V>::ResetCapacity(this->capacity_ + this->K_BLOCK_SIZE_);
  }

  this->entries_[this->num_entries_].first = column_id;
  this->entries_[this->num_entries_].second = typed_update;
  ++this->num_entries_;
  CHECK_LE(this->num_entries_, this->feature_dim_);
  // Move new entry to maintain sorted order. Always move backward.
  ml::SparseFeature<V>::OneSidedBubbleSort(this->num_entries_ - 1, false);
}

// TODO(wdai): It's the same as entry-wise Inc, except the sorting is applied
// after all applying all inc. Consider calling ApplyIncUnsafe when
// num_updates is small.
template<typename V>
void SparseFeatureRow<V>::ApplyBatchIncUnsafe(const int32_t *column_ids,
    const void* updates, int32_t num_updates) {
  const V* typed_updates = reinterpret_cast<const V*>(updates);

  // Use ApplyInc individually on each column_id.
  for (int i = 0; i < num_updates; ++i) {
    int32_t vector_idx = ml::SparseFeature<V>::FindIndex(column_ids[i]);
    if (vector_idx != -1) {
      this->entries_[vector_idx].second += typed_updates[i];

      // Remove vector_idx if vector_idx becomes 0.
      if (this->entries_[vector_idx].second == V(0)) {
        ml::SparseFeature<V>::RemoveOneEntryAndCompact(vector_idx);
      }
      continue;
    }
    // Add a new entry.
    if (this->num_entries_ == this->capacity_) {
      ml::SparseFeature<V>::ResetCapacity(this->capacity_ + this->K_BLOCK_SIZE_);
    }

    this->entries_[this->num_entries_].first = column_ids[i];
    this->entries_[this->num_entries_].second = typed_updates[i];
    ++this->num_entries_;
  }

  // Global sort.
  std::sort(this->entries_.get(), this->entries_.get() + this->num_entries_,
      [](const Entry<V>& i, const Entry<V>&j) {
      return i.first < j.first; });
}

template<typename V>
double SparseFeatureRow<V>::ApplyIncGetImportance(int32_t column_id,
    const void *update) {
  std::unique_lock<SharedMutex> write_lock(rw_mutex_);
  return ApplyIncUnsafeGetImportance(column_id, update);
}

template<typename V>
double SparseFeatureRow<V>::ApplyBatchIncGetImportance(
    const int32_t *column_ids,
    const void* updates, int32_t num_updates) {
  std::unique_lock<SharedMutex> write_lock(rw_mutex_);
  return ApplyBatchIncUnsafeGetImportance(column_ids, updates, num_updates);
}

template<typename V>
double SparseFeatureRow<V>::ApplyIncUnsafeGetImportance(int32_t column_id,
    const void *update) {
  // Go through the array and find column_id
  int32_t vector_idx = ml::SparseFeature<V>::FindIndex(column_id);
  double importance = 0.0;

  V typed_update = *(reinterpret_cast<const V*>(update));
  if (vector_idx != -1) {
    importance
      = std::abs((double(this->entries_[vector_idx].second) == 0) ?
          double(typed_update)
          : (double(typed_update) / double(this->entries_[vector_idx].second)));

    this->entries_[vector_idx].second += typed_update;

    // Remove vector_idx if vector_idx becomes 0.
    if (this->entries_[vector_idx].second == V(0)) {
      ml::SparseFeature<V>::RemoveOneEntryAndCompact(vector_idx);
    }
    return importance;
  }
  // Add a new entry.
  if (this->num_entries_ == this->capacity_) {
    ml::SparseFeature<V>::ResetCapacity(this->capacity_ + this->K_BLOCK_SIZE_);
  }

  this->entries_[this->num_entries_].first = column_id;
  this->entries_[this->num_entries_].second = typed_update;
  ++this->num_entries_;
  // Move new entry to maintain sorted order. Always move backward.
  ml::SparseFeature<V>::OneSidedBubbleSort(this->num_entries_ - 1, false);

  return std::abs(double(typed_update));
}

// TODO(wdai): It's the same as entry-wise Inc, except the sorting is applied
// after all applying all inc. Consider calling ApplyIncUnsafe when
// num_updates is small.
template<typename V>
double SparseFeatureRow<V>::ApplyBatchIncUnsafeGetImportance(
    const int32_t *column_ids,
    const void* updates, int32_t num_updates) {
  const V* typed_updates = reinterpret_cast<const V*>(updates);

  double accum_importance = 0.0;

  // Use ApplyInc individually on each column_id.
  for (int i = 0; i < num_updates; ++i) {
    int32_t vector_idx = ml::SparseFeature<V>::FindIndex(column_ids[i]);
    if (vector_idx != -1) {
      this->entries_[vector_idx].second += typed_updates[i];
      accum_importance
        += std::abs((double(this->entries_[vector_idx].second) == 0) ?
            double(typed_updates[i])
            : double(typed_updates[i]) / double(this->entries_[vector_idx].second));

      // Remove vector_idx if vector_idx becomes 0.
      if (this->entries_[vector_idx].second == V(0)) {
        ml::SparseFeature<V>::RemoveOneEntryAndCompact(vector_idx);
      }
      continue;
    }
    // Add a new entry.
    if (this->num_entries_ == this->capacity_) {
      ml::SparseFeature<V>::ResetCapacity(this->capacity_ + this->K_BLOCK_SIZE_);
    }

    this->entries_[this->num_entries_].first = column_ids[i];
    this->entries_[this->num_entries_].second = typed_updates[i];
    ++this->num_entries_;

    accum_importance += std::abs(typed_updates[i]);
  }

  // Global sort.
  std::sort(this->entries_.get(), this->entries_.get() + this->num_entries_,
      [](const Entry<V>& i, const Entry<V>&j) {
      return i.first < j.first; });

  return std::abs(accum_importance);
}

template<typename V>
double SparseFeatureRow<V>::ApplyDenseBatchIncGetImportance(
    const void* update_batch, int32_t index_st, int32_t num_updates) {
  LOG(FATAL) << "Not available";
  return 0;
}

template<typename V>
void SparseFeatureRow<V>::ApplyDenseBatchInc(
    const void* update_batch, int32_t index_st, int32_t num_updates) {
  LOG(FATAL) << "Not available";
}

template<typename V>
double SparseFeatureRow<V>::ApplyDenseBatchIncUnsafeGetImportance(
    const void* update_batch, int32_t index_st, int32_t num_updates) {
  const V* typed_updates = reinterpret_cast<const V*>(update_batch);
  double accum_importance = 0.0;

  // Use ApplyInc individually on each column_id.
  for (int i = 0; i < num_updates; ++i) {
    int32_t col_id = i + index_st;
    int32_t vector_idx = ml::SparseFeature<V>::FindIndex(col_id);
    if (vector_idx != -1) {
      this->entries_[vector_idx].second += typed_updates[i];
      accum_importance
        += std::abs((double(this->entries_[vector_idx].second) == 0) ?
            double(typed_updates[i])
            : double(typed_updates[i]) / double(this->entries_[vector_idx].second));

      // Remove vector_idx if vector_idx becomes 0.
      if (this->entries_[vector_idx].second == V(0)) {
        ml::SparseFeature<V>::RemoveOneEntryAndCompact(vector_idx);
      }
      continue;
    }
    // Add a new entry.
    if (this->num_entries_ == this->capacity_) {
      ml::SparseFeature<V>::ResetCapacity(this->capacity_ + this->K_BLOCK_SIZE_);
    }

    this->entries_[this->num_entries_].first = col_id;
    this->entries_[this->num_entries_].second = typed_updates[i];
    ++this->num_entries_;

    accum_importance += std::abs(typed_updates[i]);
  }

  // Global sort.
  std::sort(this->entries_.get(), this->entries_.get() + this->num_entries_,
      [](const Entry<V>& i, const Entry<V>&j) {
      return i.first < j.first; });

  return std::abs(accum_importance);
}

template<typename V>
void SparseFeatureRow<V>::ApplyDenseBatchIncUnsafe(
    const void* update_batch, int32_t index_st, int32_t num_updates) {
  const V* typed_updates = reinterpret_cast<const V*>(update_batch);

  // Use ApplyInc individually on each column_id.
  for (int i = 0; i < num_updates; ++i) {
    int32_t col_id = i + index_st;
    int32_t vector_idx = ml::SparseFeature<V>::FindIndex(col_id);
    if (vector_idx != -1) {
      this->entries_[vector_idx].second += typed_updates[i];

      // Remove vector_idx if vector_idx becomes 0.
      if (this->entries_[vector_idx].second == V(0)) {
        ml::SparseFeature<V>::RemoveOneEntryAndCompact(vector_idx);
      }
      continue;
    }
    // Add a new entry.
    if (this->num_entries_ == this->capacity_) {
      ml::SparseFeature<V>::ResetCapacity(this->capacity_ + this->K_BLOCK_SIZE_);
    }

    this->entries_[this->num_entries_].first = col_id;
    this->entries_[this->num_entries_].second = typed_updates[i];
    ++this->num_entries_;
  }

  // Global sort.
  std::sort(this->entries_.get(), this->entries_.get() + this->num_entries_,
      [](const Entry<V>& i, const Entry<V>&j) {
      return i.first < j.first; });
}

// ======== const_iterator Implementation ========

template<typename V>
SparseFeatureRow<V>::const_iterator::const_iterator(
    const SparseFeatureRow<V>& row, bool is_end) :
  read_lock_(row.rw_mutex_), entries_(row.entries_.get()),
  num_entries_(row.num_entries_) {
    if (is_end) {
      curr_ = num_entries_;
    } else {
      curr_ = 0;
    }
  }

template<typename V>
V SparseFeatureRow<V>::const_iterator::operator*() {
  CHECK(!is_end());
  return entries_[curr_]->second;
}

template<typename V>
typename SparseFeatureRow<V>::const_iterator::iter_t
SparseFeatureRow<V>::const_iterator::operator->() {
  CHECK(!is_end());
  return &(entries_[curr_]);
}

template<typename V>
typename SparseFeatureRow<V>::const_iterator*
SparseFeatureRow<V>::const_iterator::operator++() {
  CHECK(!is_end());
  curr_++;
  return this;
}

template<typename V>
typename SparseFeatureRow<V>::const_iterator*
SparseFeatureRow<V>::const_iterator::operator++(int unused) {
  CHECK(!is_end());
  curr_++;
  return this;
}

template<typename V>
typename SparseFeatureRow<V>::const_iterator*
SparseFeatureRow<V>::const_iterator::operator--() {
  CHECK(!is_begin());
  curr_--;
  return this;
}

template<typename V>
typename SparseFeatureRow<V>::const_iterator*
SparseFeatureRow<V>::const_iterator::operator--(int unused) {
  CHECK(!is_begin());
  curr_--;
  return this;
}

template<typename V>
bool SparseFeatureRow<V>::const_iterator::is_begin() {
  return (curr_ == 0);
}

template<typename V>
bool SparseFeatureRow<V>::const_iterator::is_end() {
  return (curr_ == num_entries_);
}

template<typename V>
typename SparseFeatureRow<V>::const_iterator
SparseFeatureRow<V>::cbegin() const {
  return const_iterator(*this, false);
}

template<typename V>
typename SparseFeatureRow<V>::const_iterator
SparseFeatureRow<V>::cend() const {
  return const_iterator(*this, true);
}

}  // namespace petuum
