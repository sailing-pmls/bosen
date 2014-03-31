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

#include <glog/logging.h>
#include "petuum_ps/client/client_table.hpp"
#include "petuum_ps/util/class_register.hpp"
#include "petuum_ps/consistency/ssp_consistency_controller.hpp"

namespace petuum {

ClientTable::ClientTable(int32_t table_id, const ClientTableConfig &config):
  table_id_(table_id), row_type_(config.table_info.row_type),
  sample_row_(ClassRegistry<AbstractRow>::GetRegistry().CreateObject(
    row_type_)),
  oplog_(table_id, config.oplog_capacity, sample_row_),
  process_storage_(config.process_cache_capacity) {
  if (GlobalContext::get_consistency_model() == SSP) {
    try {
      consistency_controller_ = new SSPConsistencyController(config.table_info,
        table_id, process_storage_, oplog_, sample_row_);
    } catch (std::bad_alloc &e) {
      LOG(FATAL) << "Bad alloc exception";
    }
  } else {
    LOG(FATAL) << "Not yet support consistency model "
      << GlobalContext::get_consistency_model();
  }
}

ClientTable::~ClientTable() {
  delete consistency_controller_;
  delete sample_row_;
}

void ClientTable::Get(int32_t row_id, RowAccessor *row_accessor) {
  return consistency_controller_->Get(row_id, row_accessor);
}

void ClientTable::Inc(int32_t row_id, int32_t column_id, const void *update) {
  return consistency_controller_->Inc(row_id, column_id, update);
}

void ClientTable::BatchInc(int32_t row_id, const int32_t* column_ids,
  const void* updates, int32_t num_updates) {
  return consistency_controller_->BatchInc(row_id, column_ids, updates,
    num_updates);
}

}  // namespace petuum
