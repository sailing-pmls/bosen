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


#include <petuum_ps_sn/client/client_table.hpp>
#include <petuum_ps_common/util/class_register.hpp>
#include <petuum_ps_common/util/stats.hpp>
#include <petuum_ps_sn/consistency/local_consistency_controller.hpp>
#include <petuum_ps_sn/consistency/local_ooc_consistency_controller.hpp>
#include <petuum_ps_sn/thread/context.hpp>
#include <cmath>
#include <glog/logging.h>
namespace petuum {

ClientTableSN::ClientTableSN(int32_t table_id, const ClientTableConfig &config):
  table_id_(table_id), row_type_(config.table_info.row_type),
  sample_row_(ClassRegistry<AbstractRow>::GetRegistry().CreateObject(
    row_type_)),
  process_storage_(config.process_cache_capacity,
                   GlobalContextSN::GetLockPoolSize(
                       config.process_cache_capacity)) {
  switch (GlobalContextSN::get_consistency_model()) {
    case LocalOOC:
        consistency_controller_ = new LocalOOCConsistencyController(config,
			    table_id, process_storage_, sample_row_,
                            thread_cache_);
        break;
    default:
      consistency_controller_ = new LocalConsistencyController(config,
                            table_id, process_storage_, sample_row_,
                            thread_cache_);
  }
}

ClientTableSN::~ClientTableSN() {
  delete consistency_controller_;
  delete sample_row_;
}

void ClientTableSN::RegisterThread() {
  if (thread_cache_.get() == 0)
    thread_cache_.reset(new ThreadTableSN(sample_row_));
}

void ClientTableSN::GetAsync(int32_t row_id) {
  consistency_controller_->GetAsync(row_id);
}

void ClientTableSN::WaitPendingAsyncGet() {
  consistency_controller_->WaitPendingAsnycGet();
}

void ClientTableSN::ThreadGet(int32_t row_id, ThreadRowAccessor *row_accessor) {
  consistency_controller_->ThreadGet(row_id, row_accessor);
}

void ClientTableSN::ThreadInc(int32_t row_id, int32_t column_id,
                            const void *update) {
  consistency_controller_->ThreadInc(row_id, column_id, update);
}
void ClientTableSN::ThreadBatchInc(int32_t row_id, const int32_t* column_ids,
                                 const void* updates,
                                 int32_t num_updates) {
  consistency_controller_->ThreadBatchInc(row_id, column_ids, updates,
    num_updates);
}

void ClientTableSN::FlushThreadCache() {
  consistency_controller_->FlushThreadCache();
}

void ClientTableSN::Get(int32_t row_id, RowAccessor *row_accessor) {
  consistency_controller_->Get(row_id, row_accessor);
}

void ClientTableSN::Inc(int32_t row_id, int32_t column_id, const void *update) {
  STATS_APP_SAMPLE_INC_BEGIN(table_id_);
  consistency_controller_->Inc(row_id, column_id, update);
  STATS_APP_SAMPLE_INC_END(table_id_);
}

void ClientTableSN::BatchInc(int32_t row_id, const int32_t* column_ids,
  const void* updates, int32_t num_updates) {
  STATS_APP_SAMPLE_BATCH_INC_BEGIN(table_id_);
  consistency_controller_->BatchInc(row_id, column_ids, updates,
                                    num_updates);
  STATS_APP_SAMPLE_BATCH_INC_END(table_id_);
}

void ClientTableSN::Clock() {
  STATS_APP_SAMPLE_CLOCK_BEGIN(table_id_);
  consistency_controller_->Clock();
  STATS_APP_SAMPLE_CLOCK_END(table_id_);
}

}  // namespace petuum
