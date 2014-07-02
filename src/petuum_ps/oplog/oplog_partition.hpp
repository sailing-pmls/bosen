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
// Author: jinliang

#pragma once

#include <stdint.h>
#include <libcuckoo/cuckoohash_map.hh>
#include <boost/unordered_map.hpp>

#include <petuum_ps_common/util/lock.hpp>
#include <petuum_ps_common/oplog/row_oplog.hpp>
#include <petuum_ps_common/util/striped_lock.hpp>
#include <petuum_ps_common/include/abstract_row.hpp>
#include <petuum_ps/thread/context.hpp>

namespace petuum {
class OpLogAccessor {
public:
  OpLogAccessor():
    row_oplog_(0) { }

  ~OpLogAccessor() { }

  Unlocker<std::mutex> *get_unlock_ptr() {
    return &unlocker_;
  }

  void set_row_oplog(RowOpLog *row_oplog) {
    row_oplog_ = row_oplog;
  }

  RowOpLog *get_row_oplog() {
    return row_oplog_;
  }

  const void* FindConst(int32_t col_id) const {
    return row_oplog_->FindConst(col_id);
  }

  void* Find(int32_t col_id) {
    return row_oplog_->Find(col_id);
  }

  void* FindCreate(int32_t col_id) {
    return row_oplog_->FindCreate(col_id);
  }

  void* BeginIterate(int32_t *column_id) {
    return row_oplog_->BeginIterate(column_id);
  }

  void* Next(int32_t *column_id) {
    return row_oplog_->Next(column_id);
  }

  const void* BeginIterateConst(int32_t *column_id) const {
    return row_oplog_->BeginIterateConst(column_id);
  }

  const void* NextConst(int32_t *column_id) const {
    return row_oplog_->NextConst(column_id);
  }

private:
  Unlocker<std::mutex> unlocker_;
  RowOpLog *row_oplog_;
};

class OpLogPartition : boost::noncopyable {
public:
  OpLogPartition();
  OpLogPartition(int32_t capacity, const AbstractRow *sample_row,
                 int32_t table_id);
  ~OpLogPartition();

  // exclusive access
  void Inc(int32_t row_id, int32_t column_id, const void *delta);
  void BatchInc(int32_t row_id, const int32_t *column_ids,
    const void *deltas, int32_t num_updates);

  // Guaranteed exclusive accesses to the same row id.
  bool FindOpLog(int32_t row_id, OpLogAccessor *oplog_accessor);
  void FindInsertOpLog(int32_t row_id, OpLogAccessor *oplog_accessor);

  // Not mutual exclusive but is less expensive than FIndOpLog above as it does
  // not use any lock.
  RowOpLog *FindOpLog(int32_t row_id);
  RowOpLog *FindInsertOpLog(int32_t row_id);

  // Mutual exclusive accesses
  typedef bool (*GetOpLogTestFunc)(RowOpLog *, void *arg);
  bool GetEraseOpLog(int32_t row_id, RowOpLog **row_oplog_ptr);
  bool GetEraseOpLogIf(int32_t row_id, GetOpLogTestFunc test,
                            void *test_args, RowOpLog **row_oplog_ptr);

private:
  RowOpLog *CreateRowOpLog();

  size_t update_size_;
  StripedLock<int32_t> locks_;
  cuckoohash_map<int32_t,  RowOpLog*> oplog_map_;
  const AbstractRow *sample_row_;
  int32_t table_id_;
};

}   // namespace petuum
