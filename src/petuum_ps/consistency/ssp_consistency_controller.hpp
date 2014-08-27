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

#include <petuum_ps_common/consistency/abstract_consistency_controller.hpp>
#include <petuum_ps/oplog/oplog.hpp>
#include <petuum_ps_common/util/vector_clock_mt.hpp>
#include <petuum_ps/client/thread_table.hpp>
#include <utility>
#include <vector>
#include <cstdint>
#include <atomic>

namespace petuum {

class SSPConsistencyController : public AbstractConsistencyController {
public:
  SSPConsistencyController(const TableInfo& info,
    int32_t table_id,
    ProcessStorage& process_storage,
    TableOpLog& oplog,
    const AbstractRow* sample_row,
    boost::thread_specific_ptr<ThreadTable> &thread_cache,
    TableOpLogIndex &oplog_index);

  virtual void GetAsync(int32_t row_id) { }
  virtual void WaitPendingAsnycGet() { }

  // Check freshness; make request and block if too stale or row_id not found
  // in storage.
  virtual void Get(int32_t row_id, RowAccessor* row_accessor);

  // Return immediately.
  virtual void Inc(int32_t row_id, int32_t column_id, const void* delta);

  virtual void BatchInc(int32_t row_id, const int32_t* column_ids, const void* updates,
			int32_t num_updates);

  virtual void ThreadGet(int32_t row_id, ThreadRowAccessor* row_accessor);

  virtual void ThreadInc(int32_t row_id, int32_t column_id, const void* delta);

  // Increment column_ids.size() entries of a row. deltas points to an array.
  virtual void ThreadBatchInc(int32_t row_id, const int32_t* column_ids,
			      const void* updates, int32_t num_updates);

  virtual void FlushThreadCache();
  virtual void Clock();

protected:
  // SSP staleness parameter.
  int32_t staleness_;

  boost::thread_specific_ptr<ThreadTable> &thread_cache_;
  TableOpLogIndex &oplog_index_;

  // Controller will only write to oplog_ but never read from it, as
  // all local updates are reflected in the row values.
  TableOpLog& oplog_;
};

}  // namespace petuum
