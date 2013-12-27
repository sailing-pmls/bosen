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

#ifndef PETUUM_LRU_ROW_STORAGE
#define PETUUM_LRU_ROW_STORAGE

#include "petuum_ps/include/configs.hpp"
#include "petuum_ps/storage/row_storage.hpp"

#include <boost/bimap.hpp>
#include <boost/bimap/list_of.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/optional.hpp>
#include <glog/logging.h>
#include <sstream>

namespace petuum {


// LRURowStorage implements a variant of least-recently-used (LRU) cache
// eviction policy called the "two-list strategy (LRU/2)". It maintains two
// lists: active_list_ and inactive_list_.  When a row is accessed once, it's
// placed in inactive_list_, wherein all the rows are candidates for eviction.
// Only when the row is accessed while it is in inactive_list_ will it be
// promoted to active_list_, which will not be considered for eviction. But if
// there are too many rows in active_list_, the least-recently used rows in
// active_list_ will be moved to inactive_list_ and subject to eviction. By
// using the inactive_list_ we avoid the "one-time-access" problem that
// happens when a row is used only once but kept in storage/cache for a long
// time.
//
// Comment(wdai): Any Put change in storage is not persistent against cache
// eviction, so make sure the data is stored on the server or other more
// persistent storage before placing in the storage here.
//
// TODO(wdai): Consider storing shared_ptr<ROW<V> > instead of ROW<V>.
template<template<typename> class ROW, typename V>
class LRURowStorage : public RowStorage<ROW, V> {
  public:
    // Access count determines how many accesses is necessary to be promoted
    // to active_list_. Store threshold count to avoid computing it all the
    // time.
    typedef typename boost::tuple<
      // <row data, current count, threshold count>
      ROW<V>, int, int> AccessStatsRow;

    // This allows us to do:
    //    AccessStatsRow r;
    //    r.template get<AccessStatsRowTuple::CURR_COUNT>()++;
    enum AccessStatsRowTuple {
      DATA = 0,
      CURR_COUNT = 1,
      THRESHOLD = 2
    };

    // Inactive_list_ needs to store row with access count.
    typedef boost::bimaps::bimap<
      boost::bimaps::unordered_set_of<int32_t>,
      boost::bimaps::list_of<AccessStatsRow> > AccessCountMapList;

    // This iterator is commonly used.
    typedef typename AccessCountMapList::left_iterator
      inactive_list_left_iter_t;

    // Active_list_ does not need additional info for the row.
    typedef boost::bimaps::bimap<
      boost::bimaps::unordered_set_of<int32_t>,
      boost::bimaps::list_of<ROW<V> > > MapList;

    // Default constructor for LRURowStorage to be initialized in container.
    LRURowStorage();

    // Basic constructor takes in max number of rows to keep.
    explicit LRURowStorage(int capacity);

    // Basic constructor takes in max number of rows to keep.
    explicit LRURowStorage(const StorageConfig& storage_config);

    // Init can be called after default constructor. Default value 0 signals
    // using internal default values.
    void Init(int capacity, int active_list_capacity = 0,
        double num_row_access_to_active = 0);

    // Allow user to specify the size of active_list_. It must be the case that
    // active_list_size < capacity.
    LRURowStorage(int capacity, int active_list_capacity);

    // Copy constructor.
    LRURowStorage(const LRURowStorage& other);

    // Additionally allow users to specify number of effective row accesses
    // needed for a row to be promoted to active_list_. Note that this number
    // needs to be >0 but not necessarily >=1.
    LRURowStorage(int capacity, int active_list_capacity,
        double num_row_access_to_active);

    // Get the current number of rows in the storage.
    int get_num_rows() const;

    // Get the size of active list. This is generally used for testing.
    int get_active_list_size() const;

    // Get the capacity of active list; used for testing.
    int get_active_list_capacity() const;

    // Put count as an access to the row and promotes it in LRU bookkeeping.
    virtual int Put(int32_t row_id, int32_t col_id, const V& val);

    // If row row_id already exists in one of the two lists, replace the row.
    // This counts as an effective row access and could promote the row to
    // active_list_ if it was not already in active_list_. If row_id is not in
    // either list, create it and place in inactive_list_ with access count =
    // 0 (ie, initialization is not access).
    virtual int PutRow(int32_t row_id, const ROW<V>& row_vals);

    // Has() does not count as one access in LRU.
    virtual bool HasRow(int32_t row_id) const;

    // Entry-wise Get increase access count by 1; promote the row to the
    // top of active_list_ if the "effective row accesses" reaches
    // num_row_access_to_active_.
    virtual int Get(int32_t row_id, int32_t col_id, V* val);
    virtual int Get(int32_t row_id, int32_t col_id, V* val, int32_t *row_iter);

    // This increase the access count by the size of the row.
    virtual int GetRow(int32_t row_id, ROW<V>* row_data, int32_t row_iter = 0);

    virtual boost::optional<ROW<V>&> GetRowUnsafe(int32_t row_id, int32_t *row_iter);

    // Return reference is only available for single-threaded implementation.
    // Since it's an 'Internal' method it does not increment the access count.
    // Use boost::optional to allow "null reference."
    //
    // Comment(wdai): This is for OpLogManager to efficiently apply op-log to
    // achieve read-my-write.
    virtual boost::optional<ROW<V>&> GetRowInternal(int32_t row_id);

    // This increases the access count by 1.
    virtual int Inc(int32_t row_id, int32_t col_id, const V& delta);

    // Eraise if row_id is in active_list_ or inactive_list_. No-op otherwise.
    virtual void Erase(int32_t row_id);

  private:
    // Insert a new row into inactive_list_, evicting other row in
    // inactive_list_ if hitting capacity. Note that we never insert a new row
    // to active_list_ in two-list strategy.
    void insert_inactive(int32_t row_id, const ROW<V>& new_row);

    // Check and promote a row from inactive_list_ to active_list if it
    // satisfies the required access counts; otherwise move it to end of
    // inactive_list_ (most recent).
    void PromoteInactiveRowOrRelocate(const inactive_list_left_iter_t& it);

    // If active_list_ exceeds active_list_capacity_, move the row at the
    // head of list (most stale) to inactive_list_ with curr_count = 0.
    //
    // Comment(wdai): We could use threshold_count as current count, which
    // will readd this row to active_list_ next time it's accessed. But
    // this has lots over overhead especially if there are more than
    // active_list_capacity_ rows being constantly accessed. So we opt to
    // start curr_count with 0.
    void CheckAndMigrateActive();

    // Since entry-wise Put(), Inc(), and Get() essentially share the same
    // implementation, use a helper function. Use mode = 0 (Get), 1 (Put), or 2
    // (Inc). in_val is ignored if mode == 0; and get_val and row_iter is not
    // set if mode != 0. Return 0 if row_id is found as specified in Put(),
    // Get(), and Inc().
    inline int PutGetIncHelper(int32_t row_id, int32_t col_id, V in_val,
        V* get_val, int32_t *row_iter, int mode);

    // Two list strategy.
    AccessCountMapList inactive_list_;
    MapList active_list_;

    // We cannot have 0 items in inactive_list_ when hitting capacity_, so we
    // impose a limit of active_list_'s size.
    int active_list_capacity_;

    // Effective row access count to promote a row in inactive_list_ to
    // active_list_.
    double num_row_access_to_active_;
};

// =============== LRURowStorage Implementation ===============

namespace {
// Default number of effective row accesses needed to promote a row from
// inactive_list_ to active_list_.
const double kDefaultNumRowAccessToActive = 1.;

// Default constant equals to (active list size / capacity). This is used
// to determine the size of active list when only capacity is specified.
const double kDefaultActiveListSizeToCapacity = 0.95;

}  // anonymous namespace

template<template<typename> class ROW, typename V>
int LRURowStorage<ROW, V>::get_num_rows() const {
  return active_list_.size() + inactive_list_.size();
}

template<template<typename> class ROW, typename V>
int LRURowStorage<ROW, V>::get_active_list_size() const {
  return active_list_.size();
}

template<template<typename> class ROW, typename V>
int LRURowStorage<ROW, V>::get_active_list_capacity() const {
  return active_list_capacity_;
}

template<template<typename> class ROW, typename V>
LRURowStorage<ROW, V>::LRURowStorage()
  : RowStorage<ROW, V>(0) {
  Init(0);
  VLOG(0) << "LRURowStorage created by default constructor.";
}

template<template<typename> class ROW, typename V>
LRURowStorage<ROW, V>::LRURowStorage(const StorageConfig& storage_config)
  : RowStorage<ROW, V>(storage_config) {
  if (storage_config.lru_params_are_defined) {
    Init(storage_config.capacity, storage_config.active_list_size,
        storage_config.num_row_access_to_active);
  } else {
    Init(storage_config.capacity);
  }
}

template<template<typename> class ROW, typename V>
LRURowStorage<ROW, V>::LRURowStorage(const LRURowStorage& other)
  : RowStorage<ROW, V>(other),
  inactive_list_(other.inactive_list_),
  active_list_(other.active_list_),
  active_list_capacity_(other.active_list_capacity_),
  num_row_access_to_active_(other.num_row_access_to_active_) { }

template<template<typename> class ROW, typename V>
LRURowStorage<ROW, V>::LRURowStorage(int capacity) {
  Init(capacity,
  // Default to 95% of the max size.
  static_cast<int>(capacity * kDefaultActiveListSizeToCapacity),
  kDefaultNumRowAccessToActive);
}

template<template<typename> class ROW, typename V>
LRURowStorage<ROW, V>::LRURowStorage(int capacity,
    int active_list_capacity) {
  Init(capacity, active_list_capacity);
}

template<template<typename> class ROW, typename V>
LRURowStorage<ROW, V>::LRURowStorage(int capacity,
    int active_list_capacity, double num_row_access_to_active)
  : RowStorage<ROW, V>(capacity) {
  Init(capacity, active_list_capacity, num_row_access_to_active);
}

template<template<typename> class ROW, typename V>
void LRURowStorage<ROW, V>::Init(int capacity, int active_list_capacity,
    double num_row_access_to_active) {
  CHECK_GE(capacity, 0);
  RowStorage<ROW, V>::set_size(capacity);
  if (active_list_capacity == 0) {
    // Using default value.
    active_list_capacity_ = static_cast<int>(capacity * kDefaultActiveListSizeToCapacity);
  } else {
    active_list_capacity_ = active_list_capacity;
  }
  if (active_list_capacity_ == capacity && capacity != 0) {
    active_list_capacity_--;
    LOG(WARNING) <<
      "Decrement active_list_capacity by 1 because active_list_capacity == capacity.";
  }

  if (num_row_access_to_active == 0) {
    num_row_access_to_active_ = kDefaultNumRowAccessToActive;
  } else {
    num_row_access_to_active_ = num_row_access_to_active;
  }

  VLOG(0) << "Initialized LRURowStorage (capacity = " << capacity
    << ", active_list_capacity = " << active_list_capacity
    << ", num_row_access_to_active = " << num_row_access_to_active;
}


template<template<typename> class ROW, typename V>
int LRURowStorage<ROW, V>::Put(int32_t row_id, int32_t col_id,
    const V& val) {
  V unused;
  int32_t unused_row_iter;
  return PutGetIncHelper(row_id, col_id, val, &unused, &unused_row_iter, 1);
}

template<template<typename> class ROW, typename V>
int LRURowStorage<ROW, V>::Get(int32_t row_id, int32_t col_id, V* val) {
  V unused = 0;
  int32_t unused_row_iter;
  return PutGetIncHelper(row_id, col_id, unused, val, &unused_row_iter, 0);
}

template<template<typename> class ROW, typename V>
int LRURowStorage<ROW, V>::Get(int32_t row_id, int32_t col_id, V* val,
    int32_t *row_iter) {
  V unused = 0;
  return PutGetIncHelper(row_id, col_id, unused, val, row_iter, 0);
}

template<template<typename> class ROW, typename V>
boost::optional<ROW<V>&> LRURowStorage<ROW, V>::GetRowUnsafe(int32_t row_id, int32_t *row_iter) {
  // Look for row_id in active_list_.
  const typename MapList::left_iterator ait =
    active_list_.left.find(row_id);
  if (ait != active_list_.left.end()) {
    // We found row_id in active_list_. Update it.
    ROW<V>& row_data = ait->second;
    *row_iter = row_data.get_iteration();

    // Now move it to the end of the list (most recent). Note that here we
    // don't need to consider access count.
    active_list_.right.relocate(
      active_list_.right.end(),
      active_list_.project_right(ait));
    return row_data;   // row_id is found and operations are applied successfully.
  }

  // Look for row_id in inactive_list_.
  const inactive_list_left_iter_t iit = inactive_list_.left.find(row_id);
  if (iit != inactive_list_.left.end()) {
    // We found row_id in inactive_list_. Update it.
    AccessStatsRow& row_tuple = iit->second;
    ROW<V>& row_data = row_tuple.template get<AccessStatsRowTuple::DATA>();
    *row_iter = row_data.get_iteration();

    // Increment the access count
    row_tuple.template get<AccessStatsRowTuple::CURR_COUNT>()++;
    VLOG(1) << "Increment row " << row_id << " access count to "
      << row_tuple.template get<AccessStatsRowTuple::CURR_COUNT>();

    // TODO(wdai): Try to promote it and still maintain reference.
    //PromoteInactiveRowOrRelocate(iit);
    return row_data;   // row_id is found and operations are applied successfully.
  }
  // The row does not exist.
  return boost::optional<ROW<V>&>();    // "null" reference.
}

template<template<typename> class ROW, typename V>
int LRURowStorage<ROW, V>::Inc(int32_t row_id, int32_t col_id,
    const V& delta) {
  V unused;
  int32_t unused_row_iter;
  return PutGetIncHelper(row_id, col_id, delta, &unused, &unused_row_iter, 2);
}

template<template<typename> class ROW, typename V>
int LRURowStorage<ROW, V>::PutGetIncHelper(int32_t row_id, int32_t col_id,
    V in_val, V* get_val, int32_t *row_iter, int mode) {
  // TODO: use enum rather than int for mode !!


  // Look for row_id in active_list_.
  const typename MapList::left_iterator ait =
    active_list_.left.find(row_id);
  if (ait != active_list_.left.end()) {
    // We found row_id in active_list_. Update it.
    ROW<V>& row_data = ait->second;
    if (mode == 0) {
      // Get
      *get_val = row_data[col_id];
      *row_iter = row_data.get_iteration();

    } else if (mode == 1) {
      // Put
      row_data[col_id] = in_val;
    } else {
      // Inc
      row_data[col_id] += in_val;
    }

    // Now move it to the end of the list (most recent). Note that here we
    // don't need to consider access count.
    active_list_.right.relocate(
      active_list_.right.end(),
      active_list_.project_right(ait));    
    return 1;   // row_id is found and operations are applied successfully.
  }

  // Look for row_id in inactive_list_.
  const inactive_list_left_iter_t iit = inactive_list_.left.find(row_id);
  if (iit != inactive_list_.left.end()) {
    // We found row_id in inactive_list_. Update it.
    AccessStatsRow& row_tuple = iit->second;
    ROW<V>& row_data = row_tuple.template get<AccessStatsRowTuple::DATA>();
    if (mode == 0) {
      // Get
      *get_val = row_data[col_id];
      *row_iter = row_data.get_iteration();

    } else if (mode == 1) {
      // Put
      row_data[col_id] = in_val;
    } else {
      // Inc
      row_data[col_id] += in_val;
    }

    // Increment the access count
    row_tuple.template get<AccessStatsRowTuple::CURR_COUNT>()++;
    VLOG(1) << "Increment row " << row_id << " access count to "
      << row_tuple.template get<AccessStatsRowTuple::CURR_COUNT>();

    PromoteInactiveRowOrRelocate(iit);
    return 1;   // row_id is found and operations are applied successfully.
  }
  
  // The row does not exist. Return 0 for No-op.
  return 0;
}

template<template<typename> class ROW, typename V>
int LRURowStorage<ROW, V>::PutRow(int32_t row_id,
    const ROW<V>& new_row) {
  // Look for row_id in active_list_.
  const typename MapList::left_iterator ait = active_list_.left.find(row_id);
  if (ait != active_list_.left.end()) {
    // We found row_id in active_list_. Update it.
    ait->second = new_row;

    // Now move it to the end of the list (most recent)
    active_list_.right.relocate(
      active_list_.right.end(),
      active_list_.project_right(ait));
    return 0;   // replace existing row.
  }

  // Look for row_id in inactive_list_.
  const inactive_list_left_iter_t iit = inactive_list_.left.find(row_id);
  if (iit != inactive_list_.left.end()) {
    // We found row_id in inactive_list_.
    AccessStatsRow& row_tuple = iit->second;

    // Update it.
    row_tuple.template get<AccessStatsRowTuple::DATA>() = new_row;
    row_tuple.template get<AccessStatsRowTuple::CURR_COUNT>() +=
      new_row.get_num_columns();
    // Recompute the threshold count.
    row_tuple.template get<AccessStatsRowTuple::THRESHOLD>() =
      static_cast<int>(num_row_access_to_active_ * new_row.get_num_columns());

    PromoteInactiveRowOrRelocate(iit);
    return 0;   // replace existing row.
  }

  // Not found in either list, put it in inactive_list_.
  insert_inactive(row_id, new_row);
  return 1;   // inserting new row.
}

template<template<typename> class ROW, typename V>
int LRURowStorage<ROW, V>::GetRow(int32_t row_id,
    ROW<V>* row_data, int32_t row_iter) {
  // Look for row_id in active_list_.
  const typename MapList::left_iterator ait = active_list_.left.find(row_id);
  if (ait != active_list_.left.end()) {
    // Now move it to the end of the list (most recent)
    active_list_.right.relocate(
      active_list_.right.end(),
      active_list_.project_right(ait));

    ROW<V>& row = ait->second;
    if (row.get_iteration() >= row_iter) {
      // We found row_id in active_list_ that's fresh enough. Read it.
      *row_data = row;
      return 1;
    }
    // Found row_id in active_list_ but too stale. No need to look for it
    // in inactive_list_.
    return 0;
  }

  // Look for row_id in inactive_list_.
  const inactive_list_left_iter_t iit = inactive_list_.left.find(row_id);
  if (iit != inactive_list_.left.end()) {
    // We found row_id in inactive_list_.
    AccessStatsRow& row_tuple = iit->second;

    // Update access count.
    row_tuple.template get<AccessStatsRowTuple::CURR_COUNT>() +=
      row_data->get_num_columns();

    // Check staleness.
    ROW<V>& row = row_tuple.template get<AccessStatsRowTuple::DATA>();
    if (row.get_iteration() >= row_iter) {
      // We found row_id in active_list_ that's fresh enough. Read it.
      *row_data = row;
      PromoteInactiveRowOrRelocate(iit);
      return 1;
    }

    // Since access count has changed, still check promotion even though it's
    // stale.
    PromoteInactiveRowOrRelocate(iit);
  }
  return 0;
}

template<template<typename> class ROW, typename V>
boost::optional<ROW<V>&> LRURowStorage<ROW, V>::GetRowInternal(int32_t row_id) {
  // Look for row_id in active_list_.
  const typename MapList::left_iterator ait = active_list_.left.find(row_id);
  if (ait != active_list_.left.end()) {
    return ait->second;
  }
  // Look for row_id in inactive_list_.
  const inactive_list_left_iter_t iit = inactive_list_.left.find(row_id);
  if (iit != inactive_list_.left.end()) {
    AccessStatsRow& row_tuple = iit->second;
    return row_tuple.template get<AccessStatsRowTuple::DATA>();
  }
  return boost::optional<ROW<V>&>();    // "null" reference.
}

template<template<typename> class ROW, typename V>
void LRURowStorage<ROW, V>::PromoteInactiveRowOrRelocate(
    const inactive_list_left_iter_t& it) {
  AccessStatsRow& row_tuple = it->second;
  int curr_count = row_tuple.template get<AccessStatsRowTuple::CURR_COUNT>();
  int threshold_count =
    row_tuple.template get<AccessStatsRowTuple::THRESHOLD>();
  if (curr_count < threshold_count) {
    // row_tuple wasn't promoted. Move it to end of inactive_list_ (most
    // recent).
    inactive_list_.right.relocate(
      inactive_list_.right.end(),
      inactive_list_.project_right(it));
    return;
  }
  // Add the new one to the end of active_list_ (most recent), as bimap's
  // list_view defaults to inserting new record at the list tail. Note
  // we don't need any count stats here.
  int32_t row_id = it->first;
  ROW<V>& row_data = row_tuple.template get<AccessStatsRowTuple::DATA>();
  active_list_.insert(
      typename MapList::value_type(row_id, row_data));
  inactive_list_.left.erase(it);

  VLOG(2) << "Promoted row " << row_id << " to active list (with count = "
    << curr_count << ").";

  // Possibly downgrade a row in active_list_ to stay under
  // active_list_capacity_.
  CheckAndMigrateActive();
}

template<template<typename> class ROW, typename V>
void LRURowStorage<ROW, V>::CheckAndMigrateActive() {
  CHECK_LE(static_cast<int>(active_list_.size()), active_list_capacity_ + 1);
  if (static_cast<int>(active_list_.size()) <= active_list_capacity_) {
    // We are good :)
    return;
  }
  // Need to migrate an active row to inactive_list_.
  const typename MapList::right_iterator rit =
    active_list_.right.begin();
  ROW<V>& last_active_row = rit->first;
  int32_t row_id = rit->second;
  int threshold_count = static_cast<int>(num_row_access_to_active_ *
    last_active_row.get_num_columns());  // Recompute the threshold count.
  AccessStatsRow new_inactive_row = boost::tuples::make_tuple(last_active_row,
      0, threshold_count);
  // Insert to the end of inactive_list_, ie., the most recently used.
  inactive_list_.insert(
    typename AccessCountMapList::value_type(row_id, new_inactive_row));

  active_list_.right.erase(rit);
}

template<template<typename> class ROW, typename V>
void LRURowStorage<ROW, V>::insert_inactive(int32_t row_id,
    const ROW<V>& new_row) {
  // Sanity check.
  int num_rows = inactive_list_.size() + active_list_.size();
  CHECK_LE(num_rows, this->capacity_)
    << "Number of rows exceed storage size.";

  // If necessary, make space...
  if (num_rows == this->capacity_) {
    // ...by purging the least-recently-used element (head of list).
    VLOG(1) << "Evicting row " << inactive_list_.right.begin()->second
      << " to insert row " << row_id;
    inactive_list_.right.erase(inactive_list_.right.begin());
  }

  // Create a new record from the key and the value
  // bimap's list_view defaults to inserting this at
  // the list tail (considered most-recently-used).
  int threshold_count = static_cast<int>(num_row_access_to_active_ *
    new_row.get_num_columns());  // Compute the threshold count.
  AccessStatsRow new_inactive_row = boost::tuples::make_tuple(new_row, 0,
      threshold_count);
  inactive_list_.insert(
    typename AccessCountMapList::value_type(row_id, new_inactive_row));
  VLOG(1) << "Inserted new row (row id = " << row_id << ") of size "
    << new_row.get_num_columns() << " with threshold count = "
    << threshold_count;
}

template<template<typename> class ROW, typename V>
bool LRURowStorage<ROW, V>::HasRow(int32_t row_id)
  const {
  // Look for row_id in active_list_.
  const typename MapList::left_const_iterator ait =
    active_list_.left.find(row_id);
  if (ait != active_list_.left.end()) {
    return true;
  }

  // Look for row_id in inactive_list_.
  const typename AccessCountMapList::left_const_iterator iit =
    inactive_list_.left.find(row_id);
  if (iit != inactive_list_.left.end()) {
    return true;
  }
  return false;
}

template<template<typename> class ROW, typename V>
void LRURowStorage<ROW, V>::Erase(int32_t row_id) {
  // Look for row_id in active_list_.
  const typename MapList::left_iterator ait =
    active_list_.left.find(row_id);
  if (ait != active_list_.left.end()) {
    // We found row_id in active_list_. Erase it.
    active_list_.left.erase(ait);
    return;
  }

  // Look for row_id in inactive_list_.
  const inactive_list_left_iter_t iit = inactive_list_.left.find(row_id);
  if (iit != inactive_list_.left.end()) {
    // We found row_id in inactive_list_. Erase it.
    inactive_list_.left.erase(iit);
  }
}

}  // namespace petuum

#endif  // PETUUM_LRU_ROW_STORAGE
