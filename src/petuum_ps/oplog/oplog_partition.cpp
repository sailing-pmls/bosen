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

#include "petuum_ps/oplog/oplog_partition.hpp"
#include "petuum_ps/thread/context.hpp"

namespace petuum {

namespace {
int kStripedLockExpansionFactor = 20;
}

OpLogPartition::OpLogPartition(int capacity, const AbstractRow *sample_row, 
  int32_t table_id):
  update_size_(sample_row->get_update_size()),
  locks_((GlobalContext::get_num_app_threads() + 1)
    *kStripedLockExpansionFactor),
  oplog_map_(capacity * GlobalContext::get_cuckoo_expansion_factor()),
  sample_row_(sample_row),
  table_id_(table_id) { }

OpLogPartition::~OpLogPartition() {
  cuckoohash_map<int32_t, RowOpLog* >::iterator iter = oplog_map_.begin();
  for(; !iter.is_end(); iter++){
    delete iter->second;
  }
}

void OpLogPartition::Inc(int32_t row_id, int32_t column_id, const void *delta) {
  locks_.Lock(row_id);
  RowOpLog *row_oplog = 0;
  if(!oplog_map_.find(row_id, row_oplog)){
    row_oplog = new RowOpLog(update_size_, sample_row_);
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
    row_oplog = new RowOpLog(update_size_, sample_row_);
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
  if(oplog_map_.find(row_id, row_oplog)){
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
  if(!oplog_map_.find(row_id, row_oplog)){
    row_oplog = new RowOpLog(update_size_, sample_row_);
    oplog_map_.insert(row_id, row_oplog);
  }
  oplog_accessor->set_row_oplog(row_oplog);
}

RowOpLog *OpLogPartition::FindOpLog(int row_id) {
  RowOpLog *row_oplog;
  if(oplog_map_.find(row_id, row_oplog)){
    return row_oplog;
  }
  return 0;
}

RowOpLog *OpLogPartition::FindInsertOpLog(int row_id) {
  RowOpLog *row_oplog;
  if(!oplog_map_.find(row_id, row_oplog)){
    row_oplog = new RowOpLog(update_size_, sample_row_);
    oplog_map_.insert(row_id, row_oplog);
  }
  return row_oplog;
}

// Memory layout of serialized OpLogs for one row:
// 1. int32_t : num of rows
// For each row
// 1. a int32_t for row id
// 2. a int32_t for number of updates
// 3. an array of int32_t: column ids for updates
// 4. an array of updates

void OpLogPartition::GetSerializedSizeByServer(
  std::map<int32_t, size_t> *num_bytes_by_server){
  
  std::vector<int32_t> server_ids = GlobalContext::get_server_ids();
  for(int i = 0; i < GlobalContext::get_num_servers(); ++i){
    int32_t server_id = server_ids[i];
    // number of rows
    (*num_bytes_by_server)[server_id] = sizeof(int32_t);
  }

  for(auto iter = oplog_map_.cbegin(); !iter.is_end(); iter++){
    int32_t row_id = iter->first;
    int32_t server_id = GlobalContext::GetRowPartitionServerID(table_id_, 
      row_id);
    RowOpLog *row_oplog_ptr = iter->second;
    int32_t num_updates = row_oplog_ptr->GetSize();
    VLOG(0) << "GetSerializedSizeByServer num_updates = " << num_updates;
    // 1) row id
    // 2) number of updates in that row
    // 3) total size for column ids
    // 4) total size for update array
    (*num_bytes_by_server)[server_id] += sizeof(int32_t) + sizeof(int32_t) +
      sizeof(int32_t)*num_updates + update_size_*num_updates;
    VLOG(0) << "Server " << server_id
	    << " size = " << (*num_bytes_by_server)[server_id];
  }
}

void OpLogPartition::SerializeByServer(
  std::map<int32_t, void* > *bytes_by_server){

  std::vector<int32_t> server_ids = GlobalContext::get_server_ids();
  std::map<int32_t, int32_t> offset_by_server;
  for(int i = 0; i < GlobalContext::get_num_servers(); ++i){
    int32_t server_id = server_ids[i];
    offset_by_server[server_id] = sizeof(int32_t);
    *((int32_t *) (*bytes_by_server)[server_id]) = 0;
  }

  for(auto iter = oplog_map_.cbegin(); !iter.is_end(); iter++){
    int32_t row_id = iter->first;
    int32_t server_id = GlobalContext::GetRowPartitionServerID(table_id_,
      row_id);
    RowOpLog *row_oplog_ptr = iter->second;
 
    uint8_t *mem = ((uint8_t *) (*bytes_by_server)[server_id]) 
      + offset_by_server[server_id];

    int32_t &mem_row_id = *((int32_t *) mem);
    mem_row_id = row_id;
    mem += sizeof(int32_t);
    
    int32_t &mem_num_updates = *((int32_t *) mem);
    mem_num_updates = row_oplog_ptr->GetSize();
    mem += sizeof(int32_t);

    int32_t num_updates = row_oplog_ptr->GetSize();

    uint8_t *mem_updates = mem + sizeof(int32_t)*num_updates;

    int32_t column_id;
    void *update = row_oplog_ptr->BeginIterate(&column_id);
    while(update != 0){
      VLOG(0) << "column_id = " << column_id
	      << " update = " << *(reinterpret_cast<int*>(update));
      int32_t &mem_column_id = *((int32_t *) mem);
      mem_column_id = column_id;
      mem += sizeof(int32_t);

      memcpy(mem_updates, update, update_size_);
      update = row_oplog_ptr->Next(&column_id);
      mem_updates += update_size_;
    }

    offset_by_server[server_id] += sizeof(int32_t) + sizeof(int32_t) + 
      (sizeof(int32_t) + update_size_)*num_updates;

    *((int32_t *) (*bytes_by_server)[server_id]) += 1;
    VLOG(0) << "Number of updates to server " << server_id
	    << " is " << *((int32_t *) (*bytes_by_server)[server_id]);
  }
  
}

}
