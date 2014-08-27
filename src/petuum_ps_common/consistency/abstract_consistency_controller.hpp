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

#include <petuum_ps_common/storage/process_storage.hpp>
#include <petuum_ps_common/include/configs.hpp>
#include <petuum_ps_common/include/abstract_row.hpp>
#include <petuum_ps_common/util/vector_clock_mt.hpp>

#include <boost/utility.hpp>
#include <cstdint>
#include <vector>

namespace petuum {

// Interface for consistency controller modules. For each table we associate a
// consistency controller (e.g., SSPController) that's essentially the "brain"
// that maintains a prescribed consistency policy upon each table action. All
// functions should be fully thread-safe.
class AbstractConsistencyController : boost::noncopyable {
public:
  // Controller modules rely on TableInfo to specify policy parameters (e.g.,
  // staleness in SSP). Does not take ownership of any pointer here.
  AbstractConsistencyController(int32_t table_id,
    ProcessStorage& process_storage,
    const AbstractRow* sample_row) :
    process_storage_(process_storage),
    table_id_(table_id),
    sample_row_(sample_row) { }

  virtual ~AbstractConsistencyController() { }

  virtual void GetAsync(int32_t row_id) = 0;
  virtual void WaitPendingAsnycGet() = 0;

  // Read a row in the table and is blocked until a valid row is obtained
  // (e.g., from server). A row is valid if, for example, it is sufficiently
  // fresh in SSP. The result is returned in row_accessor.
  virtual void Get(int32_t row_id, RowAccessor* row_accessor) = 0;

  // Increment (update) an entry. Does not take ownership of input argument
  // delta, which should be of template type UPDATE in Table. This may trigger
  // synchronization (e.g., in value-bound) and is blocked until consistency
  // is ensured.
  virtual void Inc(int32_t row_id, int32_t column_id, const void* delta) = 0;

  // Increment column_ids.size() entries of a row. deltas points to an array.
  virtual void BatchInc(int32_t row_id, const int32_t* column_ids,
    const void* updates, int32_t num_updates) = 0;

  // Read a row in the table and is blocked until a valid row is obtained
  // (e.g., from server). A row is valid if, for example, it is sufficiently
  // fresh in SSP. The result is returned in row_accessor.
  virtual void ThreadGet(int32_t row_id, ThreadRowAccessor* row_accessor) = 0;

  // Increment (update) an entry. Does not take ownership of input argument
  // delta, which should be of template type UPDATE in Table. This may trigger
  // synchronization (e.g., in value-bound) and is blocked until consistency
  // is ensured.
  virtual void ThreadInc(int32_t row_id, int32_t column_id, const void* delta)
  = 0;

  // Increment column_ids.size() entries of a row. deltas points to an array.
  virtual void ThreadBatchInc(int32_t row_id, const int32_t* column_ids,
    const void* updates, int32_t num_updates) = 0;

  virtual void FlushThreadCache() = 0;
  virtual void Clock() = 0;

protected:    // common class members for all controller modules.
  // Process cache, highly concurrent.
  ProcessStorage& process_storage_;

  int32_t table_id_;

  // We use sample_row_.AddUpdates(), SubstractUpdates() as static method.
  const AbstractRow* sample_row_;
};

}    // namespace petuum
