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
#pragma once

#include <boost/thread.hpp>
#include <vector>
#include <boost/shared_array.hpp>

namespace petuum {

// This class defines the interface of the Row type.  ApplyUpdate() and
// ApplyBatchUpdate() have to be concurrent with each other and with other
// functions that may be invoked by application threads.  Petuum system does
// not require thread safety of other functions.
class AbstractRow {
public:
  virtual ~AbstractRow() { }

  virtual void Init(int32_t capacity) = 0;

  virtual AbstractRow *Clone() const = 0;

  virtual size_t get_update_size() const = 0;

  // Upper bound of the number of bytes that serialized row shall occupy.
  // Find some balance between tightness and time complexity.
  virtual size_t SerializedSize() const = 0;

  // Bytes points to a chunk of allocated memory whose size is guaranteed to
  // be at least SerializedSize(). Need not be thread safe. Return the exact
  // size of serialized row.
  virtual size_t Serialize(void *bytes) const = 0;

  // Deserialize and initialize a row. Return true on success, false
  // otherwise. Need not be thread-safe.
  virtual bool Deserialize(const void* data, size_t num_bytes) = 0;

  // Thread safe.
  virtual void ApplyInc(int32_t column_id, const void *update) = 0;

  // Thread safe.
  // Updates are stored contiguously in the memory pointed by update_batch.
  virtual void ApplyBatchInc(const int32_t *column_ids,
    const void* update_batch, int32_t num_updates) = 0;

  // Not necessarily thread-safe.
  // PS guarantees to not call this function concurrently with other functions
  // or itself.
  virtual void ApplyIncUnsafe(int32_t column_id, const void *update) = 0;

  virtual void ApplyBatchIncUnsafe(const int32_t *column_ids,
    const void* update_batch, int32_t num_updates) = 0;

  // Aggregate update1 and update2 by summation and substraction (update1 -
  // update2), outputing to update2. column_id is optionally used in case
  // updates are applied differently for different column of a row.
  //
  // Both AddUpdates and SubstractUpdates should behave like a static
  // method.  But we cannot have virtual static method.
  // Need be thread-safe and better be concurrent.
  virtual void AddUpdates(int32_t column_id, void* update1,
    const void* update2) const = 0;

  virtual void SubtractUpdates(int32_t column_id, void *update1,
    const void* update2) const = 0;

  // Initialize update. Initialized update represents "zero update".
  // In other words, 0 + u = u (0 is the zero update).
  virtual void InitUpdate(int32_t column_id, void* zero) const = 0;
};

}   // namespace petuum
