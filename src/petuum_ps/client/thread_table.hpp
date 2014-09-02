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

#include <unordered_set>
#include <vector>
#include <boost/noncopyable.hpp>

#include <petuum_ps_common/include/abstract_row.hpp>
#include <petuum_ps_common/storage/process_storage.hpp>

#include <petuum_ps/oplog/oplog_index.hpp>
#include <petuum_ps/oplog/oplog.hpp>

namespace petuum {

class ThreadTable : boost::noncopyable {
public:
  explicit ThreadTable(const AbstractRow *sample_row);
  ~ThreadTable();
  void IndexUpdate(int32_t row_id);
  void FlushOpLogIndex(TableOpLogIndex &oplog_index);

  AbstractRow *GetRow(int32_t row_id);
  void InsertRow(int32_t row_id, const AbstractRow *to_insert);
  void Inc(int32_t row_id, int32_t column_id, const void *delta);
  void BatchInc(int32_t row_id, const int32_t *column_ids,
    const void *deltas, int32_t num_updates);

  void FlushCache(ProcessStorage &process_storage, TableOpLog &table_oplog,
		  const AbstractRow *sample_row);

private:
  std::vector<std::unordered_set<int32_t> > oplog_index_;
  boost::unordered_map<int32_t, AbstractRow* > row_storage_;
  boost::unordered_map<int32_t, RowOpLog* > oplog_map_;
  const AbstractRow *sample_row_;
};

}
