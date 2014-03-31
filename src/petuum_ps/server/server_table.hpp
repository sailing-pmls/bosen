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
// author: jinliang

#pragma once
#include "petuum_ps/server/server_row.hpp"
#include "petuum_ps/util/class_register.hpp"
#include <boost/unordered_map.hpp>
#include <map>
#include <utility>

namespace petuum {

class ServerTable : boost::noncopyable {
public:
  explicit ServerTable(const TableInfo &table_info):
    table_info_(table_info) {}

  // Move constructor: storage gets other's storage, leaving other
  // in an unspecified but valid state.
  ServerTable(ServerTable && other):
    table_info_(other.table_info_),
    storage_(std::move(other.storage_)) {}

  ServerRow *FindRow(int32_t row_id) {
    auto row_iter = storage_.find(row_id);
    if(row_iter == storage_.end())
      return 0;
    return &(row_iter->second);
  }

  ServerRow *CreateRow(int32_t row_id, RowMetadata &metadata) {
    int32_t row_type = table_info_.row_type;
    AbstractRow *row_data 
      = ClassRegistry<AbstractRow>::GetRegistry().CreateObject(row_type);
    row_data->Init(table_info_.row_capacity);
    storage_.insert(std::make_pair(row_id, ServerRow(metadata, row_data)));
    return &(storage_[row_id]);
  }

  bool ApplyRowOpLog(int32_t row_id, const int32_t *column_ids, 
    const void *updates, int32_t num_updates){
    auto row_iter = storage_.find(row_id);
    if (row_iter == storage_.end()) {
      VLOG(0) << "Row " << row_id << " is not found!";
      return false;
    }
    row_iter->second.ApplyBatchInc(column_ids, updates, num_updates);
    return true;
  }

private:
  TableInfo table_info_;
  boost::unordered_map<int32_t, ServerRow> storage_;
};

}
