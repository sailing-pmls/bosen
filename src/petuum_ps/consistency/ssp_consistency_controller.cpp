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

#include "petuum_ps/consistency/ssp_consistency_controller.hpp"
#include "petuum_ps/storage/process_storage.hpp"
#include "petuum_ps/thread/context.hpp"
#include "petuum_ps/thread/bg_workers.hpp"
#include <glog/logging.h>

namespace petuum {

SSPConsistencyController::SSPConsistencyController(const TableInfo& info,
  int32_t table_id,
  ProcessStorage& process_storage, 
  TableOpLog& oplog,
  const AbstractRow* sample_row) :
  AbstractConsistencyController(info, table_id, process_storage, oplog,
    sample_row),
  table_id_(table_id), 
  staleness_(info.table_staleness) { }

void SSPConsistencyController::Get(int32_t row_id, RowAccessor* row_accessor) {
  // Look for row_id in process_storage_.
  int32_t stalest_iter
    = GlobalContext::vector_clock.get_clock(ThreadContext::get_id())
    - staleness_;

  stalest_iter = (stalest_iter < 0) ? 0 : stalest_iter;

  if (process_storage_.Find(row_id, row_accessor)) {
    // Found it! Check staleness.
    int32_t clock = row_accessor->GetClientRow()->GetMetadata().GetClock();
    if (clock >= stalest_iter) {
      return;
    }
  }

  // Didn't find row_id that's fresh enough in process_storage_.
  // Fetch from server.
  bool found = false;
  int32_t num_fetches = 0;
  do {
    BgWorkers::RequestRow(table_id_, row_id, stalest_iter);
    // fetch again
    found = process_storage_.Find(row_id, row_accessor);
    // TODO (jinliang):
    // It's possible that the application thread does not find the row that 
    // the bg thread has just inserted. In practice, this shouldn't be an issue.
    // We'll fix it if it turns out there are too many misses.
    CHECK_LE(num_fetches, 3); // to prevent infinite loop
  }while(!found);

  CHECK_GE(row_accessor->GetClientRow()->GetMetadata().GetClock(),
    stalest_iter);
}

void SSPConsistencyController::Inc(int32_t row_id, int32_t column_id,
    const void* delta) {
  OpLogAccessor oplog_accessor;
  oplog_.Inc(row_id,column_id, delta);
  
  RowAccessor row_accessor;
  bool found = process_storage_.Find(row_id, &row_accessor);
  if (found) {
    row_accessor.GetAbstractRow()->ApplyInc(column_id, delta);
  }
}

void SSPConsistencyController::BatchInc(int32_t row_id, 
  const int32_t* column_ids, const void* updates, int32_t num_updates) {
  oplog_.BatchInc(row_id, column_ids, updates, num_updates);

  RowAccessor row_accessor;
  bool found = process_storage_.Find(row_id, &row_accessor);
  if (found) {
    row_accessor.GetAbstractRow()->ApplyBatchInc(column_ids, updates,
						 num_updates);
  }
}

}   // namespace petuum
