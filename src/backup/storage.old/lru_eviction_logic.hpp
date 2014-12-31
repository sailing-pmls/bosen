#ifndef PETUUM_EVICTION_LOGIC
#define PETUUM_EVICTION_LOGIC

#define debug_lru_unit

#include "petuum_ps/include/configs.hpp"
#include <list>
#include <cstdlib>
#include <glog/logging.h>
#include "boost/tuple/tuple.hpp"
#include <mutex>

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

#ifdef debug_lru_unit
    void print_active_list();
    void print_inactive_list();
#endif

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

    std::mutex eviction_lock_;
    std::mutex touch_lock_;
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
EvictionLogic::~EvictionLogic(){}

// private function implementations
void EvictionLogic::PromoteInactiveRowOrRelocate(list_iter_t row)
{
  int32_t row_id = boost::get<0>(*row);
  int curr_count = boost::get<1>(*row);
  if (curr_count < threshold_promote_to_active_list_)
  {
    // row_tuple wasn't promoted. Move it to end of inactive_list_ (most
    // recent).
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
  int need_demote = 0;
  // make sure there is at least 1 element in the inactive list
  if (inactive_list_.size() < 1 && active_list_.size() > 0)
  {
    need_demote = 1;
  }
  //CHECK_LE(static_cast<int>(active_list_.size()), active_list_capacity_ + 1);
  if (static_cast<int>(active_list_.size()) > active_list_capacity_) {
    need_demote = 1;
  }

  if (need_demote)
  {
    // Need to migrate an active row to inactive_list_.
    const typename ListType::iterator it = active_list_.begin();
    // moving to the inactive list, need to clear the read count
    boost::get<1>(*it) = 0;
    // Insert to the end of inactive_list_, ie., the most recently used.
    inactive_list_.splice(inactive_list_.end(),active_list_,it);
  }
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
  touch_lock_.lock();
  eviction_lock_.lock();
  inactive_list_.erase(row);
  eviction_lock_.unlock();
  touch_lock_.unlock();
  return;
}

void EvictionLogic::Touch(list_iter_t row)
{
  touch_lock_.lock();
  boost::get<1>(*row) += 1;
  PromoteInactiveRowOrRelocate(row);
  touch_lock_.unlock();
  return;
}

int32_t EvictionLogic::Get_evict()
{
  touch_lock_.lock();
  eviction_lock_.lock();
  if (inactive_list_.size() == 0)
  {
    LOG(FATAL) << "should never happen, inactive list empty";
    return -1;
  }
  const list_iter_t iit = inactive_list_.begin();
  int32_t result = boost::get<0>(*iit);
  inactive_list_.erase(iit);
  if (inactive_list_.size() == 0)
  {
    CheckAndMigrateActive();
  }
  eviction_lock_.unlock();
  touch_lock_.unlock();
  return result;
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

#ifdef debug_lru_unit
void EvictionLogic::print_active_list()
{
  list_iter_t ait = active_list_.begin();
  printf("[");
  for(; ait != active_list_.end(); ait++)
  {
    printf("(%d,%d),", boost::get<0>(*ait), boost::get<1>(*ait));
  }
  printf("]\n");
}

void EvictionLogic::print_inactive_list()
{
  list_iter_t iit = inactive_list_.begin();
  printf("[");
  for(; iit != inactive_list_.end(); iit++)
  {
    printf("(%d,%d),", boost::get<0>(*iit), boost::get<1>(*iit));
  }
  printf("]\n");
}

#endif

} // petuum name space
#endif



