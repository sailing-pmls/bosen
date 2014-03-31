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
// Date: 2014.01.20

#pragma once

#include "petuum_ps/include/row_access.hpp"
#include "petuum_ps/client/client_row.hpp"
#include "petuum_ps/util/striped_lock.hpp"
#include "petuum_ps/storage/clock_lru.hpp"
#include <libcuckoo/cuckoohash_map.hh>
#include <atomic>
#include <utility>
#include <cstdint>

namespace petuum {

// ProcessStorage is shared by all threads.
//
// TODO(wdai): Include thread storage in ProcessStorage.
class ProcessStorage {
public:
  // capacity is the upper bound of the number of rows this ProcessStorage
  // can store.
  explicit ProcessStorage(int32_t capacity);

  ~ProcessStorage();

  // Find row row_id; row_accessor is a read-only smart pointer. Return true
  // if found, false otherwise. Note that the # of active row_accessor
  // cannot be close to capacity, or Insert() will have undefined behavior
  // as we may not be able to evict any row that's not being referenced by
  // row_accessor.
  bool Find(int32_t row_id, RowAccessor* row_accessor);

  // Check if a row exists, does not count as one access
  bool Find(int32_t row_id);

  // Insert a row, and take ownership of client_row. Return true if row_id
  // does not already exist (possibly evicting another row), false if row
  // row_id already exists and is updated. If hitting capacity, then evict a
  // row using ClockLRU.  Return read reference and evicted row id if
  // row_accessor and evicted_row_id are respectively not 0. We assume
  // row_id is always non-negative, and use *evicted_row_id = -1 if no row
  // is evicted. The evicted row is guaranteed to have 0 reference count
  // (i.e., no application is using).
  //
  // Note: To stay below the capacity, we first check num_rows_. If
  // num_rows_ >= capacity_, we subtract (num_rows_ - capacity_) from
  // num_rows_ and then evict (num_rows_ - capacity_ + 1) rows using
  // ClockLRU before inserting. This could result in over-eviction when two
  // threads simultaneously do this eviction, but this is fine.
  //
  // TODO(wdai): Watch out when over-eviction clears the inactive list.
  bool Insert(int32_t row_id, ClientRow* client_row);
  bool Insert(int32_t row_id, ClientRow* client_row,
      RowAccessor* row_accessor, int32_t* evicted_row_id = 0);

private:    // private functions
  // Evict one inactive row using CLOCK replacement algorithm.
  void EvictOneInactiveRow();

  // Find row_id in storage_map_, assuming there is lock on row_id. If
  // found, update it with client_row, reference LRU, and set row_accessor
  // accordingly, and return true. Return false if row_id is not found.
  bool FindAndUpdate(int32_t row_id, ClientRow* client_row);
  bool FindAndUpdate(int32_t row_id, ClientRow* client_row,
      RowAccessor* row_accessor);

private:    // private members
  // Number of rows allowed in this storage.
  int32_t capacity_;

  // Number of rows in the storage. We choose not to use Cuckoo's size()
  // which is more expensive.
  std::atomic<int32_t> num_rows_;

  // Shared map with ClockLRU. The key type is row_id (int32_t), and the
  // value type consists of a ClientRow* pointer (void*) and a slot #
  // (int32_t).
  cuckoohash_map<int32_t, std::pair<void*, int32_t> > storage_map_;

  // Depends on storage_map_, thus need to be initialized after it.
  ClockLRU clock_lru_;

  // Lock pool.
  StripedLock<int32_t> locks_;
};


}  // namespace petuum
