// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.10.16

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <sstream>
#include <glog/logging.h>
#include <petuum_ps_common/storage/entry.hpp>
#include <ml/feature/abstract_feature.hpp>

namespace petuum {
namespace ml {

// SparseFeature stores pairs of (int32_t, V) in an array (int32_t for
// feature_id), sorted on column index in descending order. Like map, it
// supports unbounded number of items through dynamic memory allocation.
template<typename V>
class SparseFeature : public AbstractFeature<V> {
public:
  // Initialize SparseFeature to be (feature_ids[0], val[0]),
  // (feature_ids[1],val[1]) ... pairs.  feature_ids needs to be sorted, and
  // max(feature_ids[i]) < feature_dim. These are checked.
  SparseFeature(const std::vector<int32_t>& feature_ids,
      const std::vector<V>& val, int32_t feature_dim);

  SparseFeature(int32_t feature_dim);

  void Init(const std::vector<int32_t>& feature_ids,
      const std::vector<V>& val, int32_t feature_dim);

  void Init(int32_t feature_dim);

public:  // AbstractFeature override.
  SparseFeature();

  SparseFeature(const SparseFeature<V>& other);

  SparseFeature<V>& operator=(const SparseFeature<V>& other);

  virtual V operator[](int32_t feature_id) const;

  virtual void SetFeatureVal(int32_t feature_id, const V& val);

  // Number of (non-zero) entries in the underlying vector.
  virtual int32_t GetNumEntries() const {
    return num_entries_;
  }

  // Does not check idx range.
  virtual int32_t GetFeatureId(int32_t idx) const {
    return entries_[idx].first;
  }

  virtual V GetFeatureVal(int32_t idx) const {
    return entries_[idx].second;
  }

  virtual std::string ToString() const;

protected:  // protected functions
  // Comparator for std::lower_bound.
  static bool CompareIdx(const Entry<V>& a, const Entry<V>& b);

  // Return vector index of associated with feature_id, or -1 if not found.
  int32_t FindIndex(int32_t feature_id) const;

  // Move vector_idx forward (or backward) to maintain ascending order in
  // Entry.first.
  void OneSidedBubbleSort(int32_t vector_idx, bool forward);

  // Allocate new memory with new_capacity, which must be at least
  // num_entries_. block_aligned sets capacity_ to the smallest multiples of
  // K_BLOCK_SIZE_.
  void ResetCapacity(int32_t new_capacity);

  // Remove vector_idx and shift the rest forward.
  void RemoveOneEntryAndCompact(int32_t vector_idx);

public:   // static constants
  // Size to increase or shrink entries_; in # of entries. The larger it is
  // the less memory efficient, but less memory allocation.
  static const int32_t K_BLOCK_SIZE_;

protected:
  // Array of sorted entries.
  std::unique_ptr<Entry<V>[]> entries_;

  // # of entries in entries_.
  int32_t num_entries_;

  // # of entries entries_ can hold.
  int32_t capacity_;
};

// ================ Implementation =================

template<typename V>
const int32_t SparseFeature<V>::K_BLOCK_SIZE_ = 32;

template<typename V>
SparseFeature<V>::SparseFeature() :
  num_entries_(0), capacity_(0) { }

template<typename V>
SparseFeature<V>::SparseFeature(int32_t feature_dim) :
  AbstractFeature<V>(feature_dim), num_entries_(0), capacity_(0) { }

template<typename V>
void SparseFeature<V>::Init(int32_t feature_dim) {
  this->feature_dim_ = feature_dim;
  num_entries_ = 0;
  capacity_ = 0;
}

template<typename V>
SparseFeature<V>::SparseFeature(const std::vector<int32_t>& feature_ids,
    const std::vector<V>& val, int32_t feature_dim) :
  AbstractFeature<V>(feature_dim),
  entries_(new Entry<V>[feature_ids.size()]),
  num_entries_(feature_ids.size()), capacity_(num_entries_) {
    CHECK_EQ(feature_ids.size(), val.size());
    int32_t prev_idx = -1;
    for (int i = 0; i < num_entries_; ++i) {
      CHECK_LT(prev_idx, feature_ids[i]);
      prev_idx = feature_ids[i];
      entries_[i].first = feature_ids[i];
      entries_[i].second = val[i];
    }
    CHECK_LT(prev_idx, this->feature_dim_);
  }

template<typename V>
void SparseFeature<V>::Init(const std::vector<int32_t>& feature_ids,
    const std::vector<V>& val, int32_t feature_dim) {
  this->feature_dim_ = feature_dim;
  entries_.reset(new Entry<V>[feature_ids.size()]);
  num_entries_ = feature_ids.size();
  capacity_ = num_entries_;
  CHECK_EQ(feature_ids.size(), val.size());
  int32_t prev_idx = -1;
  for (int i = 0; i < num_entries_; ++i) {
    CHECK_LT(prev_idx, feature_ids[i]);
    prev_idx = feature_ids[i];
    entries_[i].first = feature_ids[i];
    entries_[i].second = val[i];
  }
  CHECK_LT(prev_idx, this->feature_dim_);
}

template<typename V>
SparseFeature<V>::SparseFeature(const SparseFeature<V>& other) :
  AbstractFeature<V>(other.feature_dim_), num_entries_(other.num_entries_),
  capacity_(other.capacity) {
    Entry<V>* new_entries = new Entry<V>[capacity_];
    memcpy(new_entries, other.entries_.get(), num_entries_ * sizeof(Entry<V>));
    entries_.reset(new_entries);
  }

template<typename V>
SparseFeature<V>& SparseFeature<V>::operator=(const SparseFeature<V>& other) {
  this->feature_dim_ = other.feature_dim_;
  num_entries_ = other.num_entries_;
  capacity_ = other.capacity_;
  Entry<V>* new_entries = new Entry<V>[capacity_];
  memcpy(new_entries, other.entries_.get(), num_entries_ * sizeof(Entry<V>));
  entries_.reset(new_entries);
  return *this;
}

template<typename V>
V SparseFeature<V>::operator[](int32_t feature_id) const {
  int32_t vector_idx = FindIndex(feature_id);
  if (vector_idx == -1)
    return V(0);
  return entries_[vector_idx].second;
}

template<typename V>
void SparseFeature<V>::SetFeatureVal(int32_t feature_id, const V& val) {
  int32_t vector_idx = FindIndex(feature_id);
  if (vector_idx != -1) {
    // Remove vector_idx if vector_idx becomes 0.
    if (val == V(0)) {
      RemoveOneEntryAndCompact(vector_idx);
      return;
    }
    entries_[vector_idx].second = val;
    return;
  }

  // Add a new entry.
  if (num_entries_ == capacity_) {
    ResetCapacity(capacity_ + K_BLOCK_SIZE_);
  }

  entries_[num_entries_].first = feature_id;
  entries_[num_entries_].second = val;
  ++num_entries_;
  // Move new entry to maintain sorted order. Always move backward.
  OneSidedBubbleSort(num_entries_ - 1, false);
}


template<typename V>
std::string SparseFeature<V>::ToString() const {
  std::stringstream ss;
  for (int i = 0; i < num_entries_; ++i) {
    ss << entries_[i].first << ":" << entries_[i].second << " ";
  }
  ss << "(feature dim: " << this->feature_dim_ << ")";
  return ss.str();
}

// ========================== Private Methods ===========================

template<typename V>
bool SparseFeature<V>::CompareIdx(const Entry<V>& a, const Entry<V>& b) {
  return a.first < b.first;
}


namespace {

// Do linear search if we have fewer than this many elements in array.
const int32_t kNumLinearSearch = 20;

// Search for index of col_id on [lower, upper).
template<typename V>
int32_t BinarySearch(const Entry<V>* entries, int32_t key, int32_t lower,
    int32_t upper) {
  if (upper - lower < kNumLinearSearch) {
    // Naive linear search.
    for (int i = lower; i < upper; ++i) {
      if (entries[i].first == key) {
        return i;
      }
    }
    return -1;
  }

  int mid = (lower + upper) / 2;
  if (key < entries[mid].first) {
    return BinarySearch(entries, key, lower, mid);
  }
  return BinarySearch(entries, key, mid, upper);
}

}  // anonymous namespace

template<typename V>
int32_t SparseFeature<V>::FindIndex(int32_t feature_id) const {
  return BinarySearch(entries_.get(), feature_id, 0, num_entries_);
}

template<typename V>
void SparseFeature<V>::OneSidedBubbleSort(int32_t vector_idx,
    bool forward) {
  if (forward) {
    // new_idx is the correct index for vector_idx.
    int32_t new_idx = vector_idx;
    int32_t feature_id = entries_[vector_idx].first;
    for (int i = vector_idx + 1; i < num_entries_; ++i) {
      if (feature_id > entries_[i].first) {
        new_idx = i;
      } else {
        break;
      }
    }
    if (new_idx > vector_idx) {
      // Move item in vector_idx to new_idx
      Entry<V> tmp = entries_[vector_idx];
      memmove(entries_.get() + vector_idx, entries_.get() + vector_idx + 1,
          (new_idx - vector_idx) * sizeof(Entry<V>));
      entries_[new_idx] = tmp;
    }
  } else {
    // Move backward
    int32_t new_idx = vector_idx;
    int32_t feature_id = entries_[vector_idx].first;
    for (int i = vector_idx - 1; i >= 0; --i) {
      if (feature_id < entries_[i].first) {
        new_idx = i;
      } else {
        break;
      }
    }
    if (new_idx < vector_idx) {
      // Move item in vector_idx to new_idx
      Entry<V> tmp = entries_[vector_idx];
      memmove(entries_.get() + new_idx + 1, entries_.get() + new_idx,
          (vector_idx - new_idx) * sizeof(Entry<V>));
      entries_[new_idx] = tmp;
    }
  }
}

template<typename V>
void SparseFeature<V>::ResetCapacity(int32_t new_capacity) {
  CHECK_GE(new_capacity, num_entries_);
  int32_t remainder = new_capacity % K_BLOCK_SIZE_;
  if (remainder != 0) {
    new_capacity += K_BLOCK_SIZE_ - remainder;
  }
  capacity_ = new_capacity;
  Entry<V>* new_entries = new Entry<V>[capacity_];
  memcpy(new_entries, entries_.get(), num_entries_ * sizeof(Entry<V>));
  entries_.reset(new_entries);
}

template<typename V>
void SparseFeature<V>::RemoveOneEntryAndCompact(int32_t vector_idx) {
  memmove(entries_.get() + vector_idx, entries_.get() + vector_idx + 1,
      (num_entries_ - vector_idx - 1) * sizeof(Entry<V>));
  --num_entries_;
  // Compact criterion.
  if (capacity_ - num_entries_ >= 2 * K_BLOCK_SIZE_) {
    // block_aligned sets capacity is to multiples of K_BLOCK_SIZE_.
    ResetCapacity(num_entries_);
  }
}

}  // namespace ml
}  // namespace petuum
