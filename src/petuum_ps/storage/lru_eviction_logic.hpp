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

#ifndef PETUUM_EVICTION_LOGIC
#define PETUUM_EVICTION_LOGIC

#include "petuum_ps/include/configs.hpp"
#include <list>
#include <cstdlib>
#include <glog/logging.h>
#include "boost/tuple/tuple.hpp"

#include <stdio.h>
#include <string.h>

namespace petuum
{

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
//
// Note(yihuaf): Iterator can be used as pointer, but not the other way around.
// TODO: (bug) destructor is giving segfault for gtest.

class EvictionLogic
{
  public:
    // <row_id, read_count>
    typedef boost::tuple<int32_t, int> ListElement;
    // Inactive_list_ needs to store row with access count.
    typedef std::list<ListElement> ListType;
    // This iterator is commonly used.
    typedef typename ListType::iterator list_iter_t;

    // touch increase the read count of the row_id by 1
    // need to update the reference
    void Touch(list_iter_t row);
    // get an row_id to be evicted
    int32_t Get_evict();
    // erase the row from the 2 lists logic
    void Erase(list_iter_t);
    // put this row into the 2-list eviction logic to keep track
    list_iter_t Put(int32_t row_id);

    // allow the user to specify the active list capacity and threshold to promote
    // to active list
    EvictionLogic(int active_list_capacity,
        int threshold_promote_to_active_list);

    // destructor
    ~EvictionLogic();

    // for testing and debugging
    int get_active_list_capacity();
    int get_threshold();

  private:

    // Insert a new row into inactive_list_, evicting other row in
    // inactive_list_ if hitting capacity. Note that we never insert a new row
    // to active_list_ in two-list strategy.
    list_iter_t insert_inactive(int32_t row_id);

    // Check and promote a row from inactive_list_ to active_list if it
    // satisfies the required access counts; otherwise move it to end of
    // inactive_list_ (most recent).
    void PromoteInactiveRowOrRelocate(list_iter_t row);

    // If active_list_ exceeds active_list_capacity_, move the row at the
    // head of list (most stale) to inactive_list_ with curr_count = 0.
    void CheckAndMigrateActive();

    // We cannot have 0 items in inactive_list_ when hitting capacity_, so we
    // impose a limit of active_list_'s size.
    int active_list_capacity_;

    // threshold of read count to promote to active list
    int threshold_promote_to_active_list_;

    // Effective row access count to promote a row in inactive_list_ to
    // active_list_.
    // double num_row_access_to_active_;

    // inactive list
    ListType inactive_list_;

    // active list
    // Comment(yihuaf): The active list does not need to keep track of the
    // read count. However, for clearity and cleaness of the implementation
    // we take the read count as a dummy variable.
    ListType active_list_;
};

// Implementation

// constructors
EvictionLogic::EvictionLogic(int active_list_capacity,
    int threshold_promote_to_active_list)
  :threshold_promote_to_active_list_(threshold_promote_to_active_list),
  active_list_capacity_(active_list_capacity)
{
  inactive_list_ = ListType();
  active_list_ = ListType();
}

// destructor
// gtest is giving me segmentation fault
EvictionLogic::~EvictionLogic()
{
  //inactive_list_.~ListType();
  //active_list_.~ListType();
}

// private function implementations
void EvictionLogic::PromoteInactiveRowOrRelocate(list_iter_t row)
{
  // we want at least 1 element inside the inactive list
  if (inactive_list_.size() < 2) return;
  int32_t row_id = boost::get<0>(*row);
  int curr_count = boost::get<1>(*row);
  if (curr_count < threshold_promote_to_active_list_)
  {
    // row_tuple wasn't promoted. Move it to end of inactive_list_ (most
    // recent).
    // Comment(yihuaf): Any better way to move element to the back?
    return inactive_list_.splice(inactive_list_.end(), inactive_list_, row);
  }
  // Add the new one to the end of active_list_ (most recent)
  active_list_.splice(active_list_.end(),inactive_list_, row);

  VLOG(2) << "Promoted row " << row_id << " to active list (with count = "
    << curr_count << ").";

  // Possibly downgrade a row in active_list_ to stay under
  // active_list_capacity_.
  CheckAndMigrateActive();
  return;
}

void EvictionLogic::CheckAndMigrateActive()
{
  //CHECK_LE(static_cast<int>(active_list_.size()), active_list_capacity_ + 1);
  if (static_cast<int>(active_list_.size()) <= active_list_capacity_) {
    // We are good :)
    return;
  }
  // Need to migrate an active row to inactive_list_.
  const typename ListType::iterator it = active_list_.begin();
  // moving to the inactive list, need to clear the read count
  boost::get<1>(*it) = 0;
  // Insert to the end of inactive_list_, ie., the most recently used.
  inactive_list_.splice(inactive_list_.end(),active_list_,it);
  // erase from the active list
  return;
}

EvictionLogic::list_iter_t EvictionLogic::insert_inactive(int32_t row_id)
{
  // Create a new record from the key and the value
  // initial read count set to 0
  VLOG(1) << "Inserted new row (row id = " << row_id << ")";
  return inactive_list_.insert(inactive_list_.end(),
      boost::make_tuple(row_id, 0));
}



// Public Methods
void EvictionLogic::Erase(list_iter_t row)
{
  inactive_list_.erase(row);
  return;
}

void EvictionLogic::Touch(list_iter_t row)
{
  boost::get<1>(*row) += 1;
  PromoteInactiveRowOrRelocate(row);
  return;
}

int32_t EvictionLogic::Get_evict()
{
  if (inactive_list_.size() == 0)
  {
    LOG(FATAL) << "inactive list is empty... should not happen";
  }
  const list_iter_t iit = inactive_list_.begin();
  return boost::get<0>(*iit);
}

EvictionLogic::list_iter_t EvictionLogic::Put(int32_t row_id)
{
  return insert_inactive(row_id);
}

int EvictionLogic::get_active_list_capacity()
{
  return active_list_capacity_;
}

int EvictionLogic::get_threshold()
{
  return threshold_promote_to_active_list_;
}

} // petuum name space
#endif



