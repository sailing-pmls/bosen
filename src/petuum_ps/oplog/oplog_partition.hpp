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

#include "petuum_ps/util/lock.hpp"
#include "petuum_ps/util/striped_lock.hpp"
#include "petuum_ps/thread/context.hpp"
#include "petuum_ps/include/abstract_row.hpp"

namespace petuum {

class RowOpLog : boost::noncopyable {
public:
  RowOpLog(uint32_t update_size, const AbstractRow *sample_row):
    update_size_(update_size),
    sample_row_(sample_row) { }

  ~RowOpLog() {
    boost::unordered_map<int32_t, void*>::iterator iter = oplogs_.begin();
    for (; iter != oplogs_.end(); iter++) {
      delete reinterpret_cast<uint8_t*>(iter->second);
    }
  }

  void* Find(int32_t col_id) {
    boost::unordered_map<int32_t, void*>::iterator iter
      = oplogs_.find(col_id);
    if (iter == oplogs_.end()) {
      return 0;
    }
    return iter->second;
  }

  void* FindCreate(int32_t col_id) {
    boost::unordered_map<int32_t, void*>::iterator iter
      = oplogs_.find(col_id);
    if (iter == oplogs_.end()) {
      void* update = reinterpret_cast<void*>(new uint8_t[update_size_]);
      sample_row_->InitUpdate(col_id, update);
      oplogs_[col_id] = update;
      return update;
    }
    return iter->second;
  }

  void* BeginIterate(int32_t *column_id) {
    iter_ = oplogs_.begin();
    if (iter_ == oplogs_.end()) {
      return 0;
    }
    *column_id = iter_->first;
    return iter_->second;
  }

  void* Next(int32_t *column_id) {
    iter_++;
    if (iter_ == oplogs_.end()) {
      return 0;
    }
    *column_id = iter_->first;
    return iter_->second;
  }

  int32_t GetSize(){
    return oplogs_.size();
  }

private:
  uint32_t update_size_;
  boost::unordered_map<int32_t, void*> oplogs_;
  const AbstractRow *sample_row_;
  boost::unordered_map<int32_t, void*>::iterator iter_;
};

class OpLogAccessor {
public:
  OpLogAccessor():
    smtx_(0),
    row_oplog_(0) { }

  ~OpLogAccessor() {
    if (smtx_ != 0) {
      smtx_->unlock();
    }
  }

  void SetPartitionLock(SharedMutex *smtx) {
    smtx_ = smtx;
  }

  Unlocker<std::mutex> *get_unlock_ptr() {
    return &unlocker_;
  }

  void set_row_oplog(RowOpLog *row_oplog) {
    row_oplog_ = row_oplog;
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

private:
  Unlocker<std::mutex> unlocker_;
  SharedMutex *smtx_;
  RowOpLog *row_oplog_;
};

class OpLogPartition : boost::noncopyable {
public:
  OpLogPartition();
  explicit OpLogPartition(int32_t capacity, const AbstractRow *sample_row, 
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

  // Return the *exact* number of bytes per server
  // Overwrites the mapped value for each server.
  void GetSerializedSizeByServer(
    std::map<int32_t, size_t> *num_bytes_by_server);

  void SerializeByServer(std::map<int32_t, void* > *bytes_by_server);

private:
  size_t update_size_;
  StripedLock<int32_t> locks_;
  cuckoohash_map<int32_t,  RowOpLog*> oplog_map_;
  const AbstractRow *sample_row_;
  int32_t table_id_;
};

}   // namespace petuum
