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
// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.01.23

#include "petuum_ps/storage/process_storage.hpp"
#include "petuum_ps/thread/context.hpp"
#include <utility>

namespace petuum {

ProcessStorage::ProcessStorage(int32_t capacity) :
  capacity_(capacity), num_rows_(0),
  storage_map_(capacity_ * GlobalContext::get_cuckoo_expansion_factor()),
  clock_lru_(capacity), locks_(GlobalContext::get_lock_pool_size()) { }

ProcessStorage::~ProcessStorage() {
  // Iterate through storage_map_ and delete client rows.
  for (auto it = storage_map_.begin(); !it.is_end(); ++it) {
    ClientRow* client_row_ptr =
      reinterpret_cast<ClientRow*>((it->second).first);
    delete client_row_ptr;
  }
}

bool ProcessStorage::Find(int32_t row_id, RowAccessor* row_accessor) {
  CHECK_NOTNULL(row_accessor);
  std::pair<void*, int32_t> row_info;
  // Lock to avoid eviction before incrementing ref count of client_row_ptr.
  Unlocker<> unlocker;
  locks_.Lock(row_id, &unlocker);
  bool found = storage_map_.find(row_id, row_info);
  if (found) {
    CHECK_NOTNULL(row_info.first);
    ClientRow* client_row_ptr = reinterpret_cast<ClientRow*>(row_info.first);
    // SetClientRow() increments the ref count and needs to be protected by
    // lock.
    row_accessor->SetClientRow(client_row_ptr);
    clock_lru_.Reference(row_info.second);
    return true;
  }
  return false;
}

bool ProcessStorage::Find(int32_t row_id) {
  std::pair<void*, int32_t> row_info;
  bool found = storage_map_.find(row_id, row_info);
  if (found)
    return true;
  return false;
}

bool ProcessStorage::Insert(int32_t row_id, ClientRow* client_row) {
  { // Look for row_id. Lock row_id so no other insert can take place.
    Unlocker<> unlocker;
    locks_.Lock(row_id, &unlocker);
    if (FindAndUpdate(row_id, client_row)) {
      // row_id is found, LRU is reference and client row updated.
      return false;
    }
  }   // release lock on row_id.

  // row_id does not exist in storage. Check space and evict if necessary.
  if (capacity_ - (++num_rows_) < 0) {
    --num_rows_;  // We are evicting one row now.
    while (true) {
      int32_t evict_candidate = clock_lru_.FindOneToEvict();
      // Lock to prevent concurrent insert on evict_candidate.
      Unlocker<> unlocker;
      locks_.Lock(evict_candidate, &unlocker);
      std::pair<void*, int32_t> row_info;
      CHECK(storage_map_.find(evict_candidate, row_info));

      ClientRow* candidate_client_row_ptr =
        reinterpret_cast<ClientRow*>(row_info.first);
      if (candidate_client_row_ptr->HasZeroRef()) {
        // erase() and Evict() can be called in either order.
        delete candidate_client_row_ptr;
        storage_map_.erase(evict_candidate);
        clock_lru_.Evict(row_info.second);
        break;
      } else {
        // Can't evict with non-zero ref count.
        clock_lru_.NoEvict(row_info.second);
      }
    }
  }
  { // Lock again. This time we can insert for sure.
    Unlocker<> unlocker;
    locks_.Lock(row_id, &unlocker);
    if (FindAndUpdate(row_id, client_row)) {
      // row_id is found, LRU referenced, and client row updated. Other thread
      // must have inserted while we lost lock on row_id.
      --num_rows_;   // Since we anticipated insertion and added 1 to num_rows_.
      return false;
    }
    // Now we can insert row_id without worrying exceeding capacity.
    std::pair<void*, int32_t> row_info;
    row_info.first = reinterpret_cast<void*>(client_row);
    row_info.second = clock_lru_.Insert(row_id);
    CHECK(storage_map_.insert(row_id, row_info));
  }
  return true;
}

bool ProcessStorage::Insert(int32_t row_id, ClientRow* client_row,
    RowAccessor* row_accessor, int32_t* evicted_row_id) {
  CHECK_NOTNULL(row_accessor);
  if (evicted_row_id != 0) {
    *evicted_row_id = -1;
  }

  { // Look for row_id. Lock row_id so no other insert can take place.
    Unlocker<> unlocker;
    locks_.Lock(row_id, &unlocker);
    if (FindAndUpdate(row_id, client_row, row_accessor)) {
      // row_id is found, LRU referenced, and client row updated.
      return false;
    }
  }   // release lock on row_id.

  // row_id does not exist in storage. Check space and evict if necessary.
  if (capacity_ - (++num_rows_) < 0) {
    --num_rows_;  // We are evicting one row now.
    while (true) {
      int32_t evict_candidate = clock_lru_.FindOneToEvict();
      // Lock to prevent concurrent insert on evict_candidate.
      Unlocker<> unlocker;
      locks_.Lock(evict_candidate, &unlocker);
      std::pair<void*, int32_t> row_info;
      CHECK(storage_map_.find(evict_candidate, row_info))
        << "row " << evict_candidate << "cannot possibly be evicted while "
        << "the lock on the slot for evict_candidate is held. Report bug.";
      ClientRow* candidate_client_row_ptr =
        reinterpret_cast<ClientRow*>(row_info.first);
      if (candidate_client_row_ptr->HasZeroRef()) {
        // erase() and Evict() can be called in either order.
        delete candidate_client_row_ptr;
        storage_map_.erase(evict_candidate);
        clock_lru_.Evict(row_info.second);
        if (evicted_row_id != 0) {
          *evicted_row_id = evict_candidate;
        }
        break;
      } else {
        // Can't evict with non-zero ref count.
        clock_lru_.NoEvict(row_info.second);
      }
    }
  }

  { // Lock again. This time we can insert for sure.
    Unlocker<> unlocker;
    locks_.Lock(row_id, &unlocker);
    if (FindAndUpdate(row_id, client_row, row_accessor)) {
      // row_id is found, LRU referenced, and client row updated. Other thread
      // must have inserted while we lost lock on row_id.
      --num_rows_;   // Since we anticipated insertion and added 1 to num_rows_.
      return false;
    }

    // Now we can insert row_id without worrying exceeding capacity.
    std::pair<void*, int32_t> row_info;
    row_info.first = reinterpret_cast<void*>(client_row);
    row_info.second = clock_lru_.Insert(row_id);
    CHECK(storage_map_.insert(row_id, row_info));
    row_accessor->SetClientRow(client_row);
  }
  return true;
}

// ==================== Private Methods ======================

bool ProcessStorage::FindAndUpdate(int32_t row_id, ClientRow* client_row) {
  std::pair<void*, int32_t> row_info;
  bool found = storage_map_.find(row_id, row_info);
  if (found) {
    CHECK_NOTNULL(row_info.first);
    // Update the client row.
    ClientRow* client_row_ptr =
      reinterpret_cast<ClientRow*>(row_info.first);
    clock_lru_.Reference(row_info.second);
    client_row_ptr->SwapAndDestroy(client_row);
    return true;
  }
  return false;
}

bool ProcessStorage::FindAndUpdate(int32_t row_id, ClientRow* client_row,
    RowAccessor* row_accessor) {
  std::pair<void*, int32_t> row_info;
  bool found = storage_map_.find(row_id, row_info);
  if (found) {
    CHECK_NOTNULL(row_info.first);
    // Update the client row.
    ClientRow* client_row_ptr =
      reinterpret_cast<ClientRow*>(row_info.first);
    client_row_ptr->SwapAndDestroy(client_row);
    row_accessor->SetClientRow(client_row_ptr);
    clock_lru_.Reference(row_info.second);
    return true;
  }
  return false;
}

}  // namespace petuum
