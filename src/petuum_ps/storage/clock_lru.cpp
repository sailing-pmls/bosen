// Copyright (c) 2014, Sailing Lab
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
#include "petuum_ps/storage/clock_lru.hpp"
#include "petuum_ps/thread/context.hpp"

namespace petuum {

const int32_t ClockLRU::MAX_NUM_ROUNDS = 3;

ClockLRU::ClockLRU(int capacity) :
  capacity_(capacity), evict_hand_(0), insert_hand_(0),
  locks_(GlobalContext::get_lock_pool_size()),
  stale_(new std::atomic_flag[capacity]),
  row_ids_(capacity) {
    for (int i = 0; i < capacity_; ++i) {
      // Default constructor for atomic_flag initialize to unspecified state.
      stale_[i].test_and_set();
      row_ids_[i] = -1;
    }
  }

int32_t ClockLRU::FindOneToEvict() {
  for (int i = 0; i < MAX_NUM_ROUNDS * capacity_; ++i) {
    // Check slot pointed by evict_hand_ and increment it so other thread will
    // not check this slot immediately.
    int32_t slot = evict_hand_++ % capacity_;

    // Check recency.
    if (!stale_[slot].test_and_set()) {
      // slot is recent. Set it to stale and skip it.
      continue;
    }
    // We've found a slot that is not recently used (stale).

    // If we can't get lock, then move on.
    Unlocker<SpinMutex> unlocker;
    if (!locks_.TryLock(slot, &unlocker)) {
      continue;
    }
    // stale_[slot] can still be changed after locking, but we don't care.

    // Check occupancy of the slot.
    if (row_ids_[slot] == -1) {
      // Got an empty slot. Duh...
      continue;
    }

    // Found it! Release the lock from unlocker to keep the lock on. slot will
    // be unlocked in Evict() or NoEvict().
    unlocker.Release();
    return row_ids_[slot];
  }
  LOG(FATAL) << "Cannot find a slot to evict after the clock hand goes "
    << MAX_NUM_ROUNDS << " rounds.";
}

void ClockLRU::Evict(int32_t slot) {
  // We assume we are holding lock on the slot.
  row_ids_[slot] = -1;
  empty_slots_.push(slot);
  locks_.Unlock(slot);
}

void ClockLRU::NoEvict(int32_t slot) {
  locks_.Unlock(slot);
}

int32_t ClockLRU::Insert(int32_t row_id) {
  Unlocker<SpinMutex> unlocker;
  int32_t slot = FindEmptySlot(&unlocker);
  stale_[slot].clear();
  row_ids_[slot] = row_id;
  return slot;
}

void ClockLRU::Reference(int32_t slot) {
  stale_[slot].clear();
}

int32_t ClockLRU::FindEmptySlot(
    Unlocker<SpinMutex> *unlocker) {
  CHECK_NOTNULL(unlocker);
  // Check empty_slots_.
  int32_t slot;
  if (empty_slots_.pop(&slot) > 0) {
    // empty_slots_ has something.
    // This lock should eventually suceed.
    Unlocker<SpinMutex> tmp_unlocker;
    locks_.Lock(slot, &tmp_unlocker);
    CHECK_EQ(-1, row_ids_[slot]) << "insert_hand_ cannot take slot "
      << "from empty_slots_ queue. Report bug.";
    // Transfer lock
    unlocker->SetLock(tmp_unlocker.GetAndRelease());
    return slot;
  }

  // Going around the clock to look for space is only used in the first round.
  // Afterwards (insert_hand_val_ >= capacity) we know we are out of space.
  int32_t insert_hand_val = insert_hand_++;
  if (insert_hand_val >= capacity_) {
    LOG(FATAL) << "Exceeding ClockLRU capacity. Report Bug";
  }
  // The first round will always succeed at insert_hand_val.
  slot = insert_hand_val % capacity_;
  // Lock to ensure slot stays empty.
  Unlocker<SpinMutex> tmp_unlocker;
  locks_.Lock(slot, &tmp_unlocker);
  CHECK_EQ(-1, row_ids_[slot])
    << "This is first round and must be empty. Report bug.";
  // Transfer lock
  unlocker->SetLock(tmp_unlocker.GetAndRelease());
  return slot;
}

bool ClockLRU::HasRow(int32_t row_id, int32_t slot) {
  return (row_ids_[slot] == row_id);
}

}  // namespace petuum
