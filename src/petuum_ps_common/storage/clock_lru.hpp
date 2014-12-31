// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.01.24

#pragma once

#include <vector>
#include <atomic>
#include <cstdint>
#include <memory>
#include <boost/scoped_array.hpp>

#include <petuum_ps_common/util/striped_lock.hpp>
#include <petuum_ps_common/util/lock.hpp>
#include <petuum_ps_common/util/mt_queue.hpp>

namespace petuum {

// ClockLRU implements the CLOCK replacement algorithm to approximate Least
// Recently Used (LRU) policy. Fully thread-safe.
//
// Comment(wdai): We cannot share StripedLock with ProcessStorage because the
// lock here has to be based on slot # (not row_id).
class ClockLRU {
public:
  explicit ClockLRU(int capacity, size_t lock_pool_size);

  // Find an (infrequently used) row_id, but do not kick it out yet. Return
  // the row_id which is locked to prevent erase or insert (but not
  // refreshing) before user comes back to evict it or unlock it (the row has
  // positive reference count and can't be evicted). FindOneToEvict will
  // either return or fail after searching for MAX_NUM_ROUNDS times.
  int32_t FindOneToEvict();

  // User must call Evict or NoEvict after FindOneToEvict to unlock the slot.
  // Note that Reference() called on row_id during Evict() could fail (no-op).
  // The number of occupied slots minus the number of ongoing Evict()
  // invocation need to be greater or equal to 0.
  void Evict(int32_t slot);
  void NoEvict(int32_t slot);

  // Insert a row and set it to recent. row_id must not already have a slot #
  // (this is not checked). Return the slot # associated with row_id. The
  // number of occupied slots plus the number of ongoing Insert() should be
  // less or equal to capacity.
  int32_t Insert(int32_t row_id);

  // Reference a row (i.e., row_id is used) by the slot #.
  void Reference(int32_t slot);

  // For testing purpose; not part of standard LRU interface.
  bool HasRow(int32_t row_id, int32_t slot);

  // Going around the clock at most MAX_NUM_ROUNDS times when looking for
  // eviction.
  static const int32_t MAX_NUM_ROUNDS;

private:    // private functions
  // Return the slot found, which is guaranteed to be available as long as
  // unlocker is alive. row_id_ won't be set for the returned slot # (still
  // -1), which needs to be set in Insert(). Fail the program if can't find
  // any slot.
  int32_t FindEmptySlot(Unlocker<SpinMutex>* unlocker);

private:    // private members
  const int32_t capacity_;

  // Point to the candidate slot for next evict attempt.
  std::atomic<int32_t> evict_hand_;

  // Point to the candidate slot for next insert attempt. This is only used
  // at the beginning, before the buffer is filled up. Afterwards the
  // empty_slots_ will assist finding empty slot.
  std::atomic<int32_t> insert_hand_;

  // Evict() will put the freed slot # to empty_slots_, which is protected by
  // empty_slots_lock_.
  MTQueue<int32_t> empty_slots_;

  // A thread locks a slot when performing insertion and erasure.
  //
  // Note that referencing an existing slot does not need to lock. Reference
  // during insertion or erasure could fail (which is okay).
  StripedLock<int32_t, SpinMutex> locks_;

  // staled_[i] is set to false if the row in slot i is referenced. It's
  // set to true when the evict_hand_ comes around.
  std::unique_ptr<std::atomic_flag[]> stale_;

  // Store associated row_id needed during eviction. -1 implies empty.
  std::vector<int32_t> row_ids_;
};

}  // namespace petuum
