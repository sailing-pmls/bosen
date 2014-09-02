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
#include <petuum_ps/client/client_table.hpp>
#include <petuum_ps_common/util/class_register.hpp>
#include <petuum_ps_common/util/stats.hpp>
#include <petuum_ps/consistency/ssp_consistency_controller.hpp>
#include <petuum_ps/consistency/ssp_push_consistency_controller.hpp>
#include <petuum_ps/thread/context.hpp>
#include <cmath>

namespace petuum {

ClientTable::ClientTable(int32_t table_id, const ClientTableConfig &config):
    AbstractClientTable(),
  table_id_(table_id), row_type_(config.table_info.row_type),
  sample_row_(ClassRegistry<AbstractRow>::GetRegistry().CreateObject(
    row_type_)),
  oplog_(table_id, std::ceil(static_cast<float>(config.oplog_capacity)
      / GlobalContext::get_num_bg_threads()), sample_row_),
    process_storage_(config.process_cache_capacity,
                     GlobalContext::GetLockPoolSize()),
  oplog_index_(std::ceil(static_cast<float>(config.oplog_capacity)
     / GlobalContext::get_num_bg_threads())) {
  switch (GlobalContext::get_consistency_model()) {
    case SSP:
      {
        consistency_controller_
            = new SSPConsistencyController(config.table_info,
              table_id, process_storage_, oplog_, sample_row_, thread_cache_,
              oplog_index_);
      }
      break;
    case SSPPush:
      {
        consistency_controller_
            = new SSPPushConsistencyController(config.table_info,
              table_id, process_storage_, oplog_, sample_row_, thread_cache_,
              oplog_index_);
      }
      break;
    default:
      LOG(FATAL) << "Not yet support consistency model "
                 << GlobalContext::get_consistency_model();

  }
}

ClientTable::~ClientTable() {
  delete consistency_controller_;
  delete sample_row_;
}

void ClientTable::RegisterThread() {
  if (thread_cache_.get() == 0)
    thread_cache_.reset(new ThreadTable(sample_row_));
}

void ClientTable::GetAsync(int32_t row_id) {
  consistency_controller_->GetAsync(row_id);
}

void ClientTable::WaitPendingAsyncGet() {
  consistency_controller_->WaitPendingAsnycGet();
}

void ClientTable::ThreadGet(int32_t row_id, ThreadRowAccessor *row_accessor) {
  consistency_controller_->ThreadGet(row_id, row_accessor);
}

void ClientTable::ThreadInc(int32_t row_id, int32_t column_id,
                            const void *update) {
  consistency_controller_->ThreadInc(row_id, column_id, update);
}
void ClientTable::ThreadBatchInc(int32_t row_id, const int32_t* column_ids,
                                 const void* updates,
                                 int32_t num_updates) {
  consistency_controller_->ThreadBatchInc(row_id, column_ids, updates,
    num_updates);
}

void ClientTable::FlushThreadCache() {
  consistency_controller_->FlushThreadCache();
}

void ClientTable::Get(int32_t row_id, RowAccessor *row_accessor) {
  consistency_controller_->Get(row_id, row_accessor);
}

void ClientTable::Inc(int32_t row_id, int32_t column_id, const void *update) {
  STATS_APP_SAMPLE_INC_BEGIN(table_id_);
  consistency_controller_->Inc(row_id, column_id, update);
  STATS_APP_SAMPLE_INC_END(table_id_);
}

void ClientTable::BatchInc(int32_t row_id, const int32_t* column_ids,
  const void* updates, int32_t num_updates) {
  STATS_APP_SAMPLE_BATCH_INC_BEGIN(table_id_);
  consistency_controller_->BatchInc(row_id, column_ids, updates,
                                    num_updates);
  STATS_APP_SAMPLE_BATCH_INC_END(table_id_);
}

void ClientTable::Clock() {
  STATS_APP_SAMPLE_CLOCK_BEGIN(table_id_);
  consistency_controller_->Clock();
  STATS_APP_SAMPLE_CLOCK_END(table_id_);
}

cuckoohash_map<int32_t, bool> *ClientTable::GetAndResetOpLogIndex(
    int32_t partition_num) {
  return oplog_index_.ResetPartition(partition_num);
}

}  // namespace petuum
