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

#include <petuum_ps/oplog/oplog_index.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps_common/include/constants.hpp>
#include <glog/logging.h>

namespace petuum {

PartitionOpLogIndex::PartitionOpLogIndex(size_t capacity):
    capacity_(capacity),
    locks_(GlobalContext::GetLockPoolSize()),
    shared_oplog_index_(new cuckoohash_map<int32_t, bool>
                        (capacity*kCuckooExpansionFactor)){
  VLOG(0) << "Constructor shared_oplog_index = " << shared_oplog_index_;
}

PartitionOpLogIndex::~PartitionOpLogIndex() {
  if (shared_oplog_index_ != 0)
    delete shared_oplog_index_;
}


PartitionOpLogIndex::PartitionOpLogIndex(PartitionOpLogIndex && other):
  capacity_(other.capacity_),
  shared_oplog_index_(other.shared_oplog_index_) {
  other.shared_oplog_index_ = 0;
}

void PartitionOpLogIndex::AddIndex(const std::unordered_set<int32_t>
                                   &oplog_index) {
  smtx_.lock_shared();
  for (auto iter = oplog_index.cbegin(); iter != oplog_index.cend(); iter++) {
    locks_.Lock(*iter);
    shared_oplog_index_->insert(*iter, true);
    locks_.Unlock(*iter);
  }
  smtx_.unlock_shared();
}

cuckoohash_map<int32_t, bool> *PartitionOpLogIndex::Reset() {
  smtx_.lock();
  cuckoohash_map<int32_t, bool> *old_index = shared_oplog_index_;
  shared_oplog_index_ = new cuckoohash_map<int32_t, bool>
                    (capacity_*kCuckooExpansionFactor);
  smtx_.unlock();
  return old_index;
}

TableOpLogIndex::TableOpLogIndex(size_t capacity) {
  for (int32_t i = 0; i < GlobalContext::get_num_bg_threads(); ++i) {
    partition_oplog_index_.emplace_back(capacity);
  }
}

void TableOpLogIndex::AddIndex(int32_t partition_num,
                               const std::unordered_set<int32_t>
                               &oplog_index) {

  partition_oplog_index_[partition_num].AddIndex(oplog_index);
}

cuckoohash_map<int32_t, bool> *TableOpLogIndex::ResetPartition(
    int32_t partition_num) {
  return partition_oplog_index_[partition_num].Reset();
}

}
