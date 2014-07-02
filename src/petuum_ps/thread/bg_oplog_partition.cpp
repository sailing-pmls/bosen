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

#include <petuum_ps/thread/bg_oplog_partition.hpp>
#include <petuum_ps/thread/context.hpp>

namespace petuum {

BgOpLogPartition::BgOpLogPartition() { }

BgOpLogPartition::BgOpLogPartition(int32_t table_id, size_t update_size):
    table_id_(table_id),
    update_size_(update_size) { }

BgOpLogPartition::~BgOpLogPartition() {
  //VLOG(0) << "Destroy BgOpLogPartition()";
  for(auto iter = oplog_map_.begin(); iter != oplog_map_.end(); iter++){
    delete iter->second;
  }
}

RowOpLog *BgOpLogPartition::FindOpLog(int row_id) {
  boost::unordered_map<int32_t, RowOpLog*>::iterator oplog_iter
      = oplog_map_.find(row_id);
  if (oplog_iter != oplog_map_.end())
    return oplog_iter->second;
  return 0;
}

void BgOpLogPartition::InsertOpLog(int row_id, RowOpLog *row_oplog) {
  oplog_map_[row_id] = row_oplog;
  //VLOG(0) << "Inserted row " << row_id << " to oplog partition";
}

void BgOpLogPartition::SerializeByServer(
  std::map<int32_t, void* > *bytes_by_server){

  std::vector<int32_t> &server_ids = GlobalContext::get_server_ids();
  std::map<int32_t, int32_t> offset_by_server;
  for(int i = 0; i < GlobalContext::get_num_servers(); ++i){
    int32_t server_id = server_ids[i];
    offset_by_server[server_id] = sizeof(int32_t);
    // Init number of rows to 0
    *((int32_t *) (*bytes_by_server)[server_id]) = 0;
  }

  for(auto iter = oplog_map_.cbegin(); iter != oplog_map_.cend(); iter++){
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
    const void *update = row_oplog_ptr->BeginIterateConst(&column_id);
    while(update != 0){
      int32_t &mem_column_id = *((int32_t *) mem);
      mem_column_id = column_id;
      mem += sizeof(int32_t);

      memcpy(mem_updates, update, update_size_);
      update = row_oplog_ptr->NextConst(&column_id);
      mem_updates += update_size_;
    }

    offset_by_server[server_id] += sizeof(int32_t) + sizeof(int32_t) +
      (sizeof(int32_t) + update_size_)*num_updates;

    *((int32_t *) (*bytes_by_server)[server_id]) += 1;
  }
}

}
