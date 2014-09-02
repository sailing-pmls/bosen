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
#include <petuum_ps/oplog/oplog_partition.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps_common/include/constants.hpp>

#include <functional>

namespace petuum {

OpLogPartition::OpLogPartition(int capacity, const AbstractRow *sample_row,
                               int32_t table_id):
  update_size_(sample_row->get_update_size()),
  locks_(GlobalContext::GetLockPoolSize()),
  oplog_map_(capacity * kCuckooExpansionFactor),
  sample_row_(sample_row),
  table_id_(table_id) { }

OpLogPartition::~OpLogPartition() {
  cuckoohash_map<int32_t, RowOpLog* >::iterator iter = oplog_map_.begin();
  for(; !iter.is_end(); iter++){
    delete iter->second;
  }
}

RowOpLog *OpLogPartition::CreateRowOpLog() {
  return new RowOpLog(update_size_,
                      std::bind(&AbstractRow::InitUpdate,
                                sample_row_, std::placeholders::_1,
                                std::placeholders::_2));
}

void OpLogPartition::Inc(int32_t row_id, int32_t column_id, const void *delta) {
  locks_.Lock(row_id);
  RowOpLog *row_oplog = 0;
  if(!oplog_map_.find(row_id, row_oplog)){
    row_oplog = CreateRowOpLog();
    oplog_map_.insert(row_id, row_oplog);
  }

  void *oplog_delta = row_oplog->FindCreate(column_id);
  sample_row_->AddUpdates(column_id, oplog_delta, delta);
  locks_.Unlock(row_id);
}

void OpLogPartition::BatchInc(int32_t row_id, const int32_t *column_ids,
  const void *deltas, int32_t num_updates) {
  locks_.Lock(row_id);
  RowOpLog *row_oplog = 0;
  if(!oplog_map_.find(row_id, row_oplog)){
    row_oplog = CreateRowOpLog();
    oplog_map_.insert(row_id, row_oplog);
  }

  const uint8_t* deltas_uint8 = reinterpret_cast<const uint8_t*>(deltas);

  for (int i = 0; i < num_updates; ++i) {
    void *oplog_delta = row_oplog->FindCreate(column_ids[i]);
    sample_row_->AddUpdates(column_ids[i], oplog_delta, deltas_uint8
      + update_size_*i);
  }
  locks_.Unlock(row_id);
}

bool OpLogPartition::FindOpLog(int32_t row_id, OpLogAccessor *oplog_accessor) {
  locks_.Lock(row_id, oplog_accessor->get_unlock_ptr());
  RowOpLog *row_oplog;
  if (oplog_map_.find(row_id, row_oplog)) {
    oplog_accessor->set_row_oplog(row_oplog);
    return true;
  }
  (oplog_accessor->get_unlock_ptr())->Release();
  locks_.Unlock(row_id);
  return false;
}

void OpLogPartition::FindInsertOpLog(int32_t row_id,
  OpLogAccessor *oplog_accessor) {
  locks_.Lock(row_id, oplog_accessor->get_unlock_ptr());
  RowOpLog *row_oplog;
  if (!oplog_map_.find(row_id, row_oplog)) {
    row_oplog = CreateRowOpLog();
    oplog_map_.insert(row_id, row_oplog);
  }
  oplog_accessor->set_row_oplog(row_oplog);
}

RowOpLog *OpLogPartition::FindOpLog(int row_id) {
  RowOpLog *row_oplog;
  if (oplog_map_.find(row_id, row_oplog)) {
    return row_oplog;
  }
  return 0;
}

RowOpLog *OpLogPartition::FindInsertOpLog(int row_id) {
  RowOpLog *row_oplog;
  if (!oplog_map_.find(row_id, row_oplog)) {
    row_oplog = CreateRowOpLog();
    oplog_map_.insert(row_id, row_oplog);
  }
  return row_oplog;
}

bool OpLogPartition::GetEraseOpLog(int32_t row_id,
                                   RowOpLog **row_oplog_ptr) {
  Unlocker<> unlocker;
  locks_.Lock(row_id, &unlocker);
  RowOpLog *row_oplog = 0;
  if (!oplog_map_.find(row_id, row_oplog)) {
    return false;
  }
  oplog_map_.erase(row_id);
  *row_oplog_ptr = row_oplog;
  return true;
}

bool OpLogPartition::GetEraseOpLogIf(int32_t row_id, GetOpLogTestFunc test,
                                     void *test_args,
                                     RowOpLog **row_oplog_ptr) {
  Unlocker<> unlocker;
  locks_.Lock(row_id, &unlocker);
  RowOpLog *row_oplog = 0;
  if (!oplog_map_.find(row_id, row_oplog)) {
    return false;
  }

  if (!test(row_oplog, test_args)) {
    *row_oplog_ptr = 0;
    return true;
  }

  oplog_map_.erase(row_id);
  *row_oplog_ptr = row_oplog;
  return true;
}

}
