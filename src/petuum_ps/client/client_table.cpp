
#include <glog/logging.h>
#include "petuum_ps/client/client_table.hpp"
#include "petuum_ps/util/class_register.hpp"
#include "petuum_ps/util/stats.hpp"
#include "petuum_ps/consistency/ssp_consistency_controller.hpp"
#include "petuum_ps/consistency/ssp_push_consistency_controller.hpp"
#include "petuum_ps/thread/context.hpp"
#include <cmath>

namespace petuum {

ClientTable::ClientTable(int32_t table_id, const ClientTableConfig &config):
  table_id_(table_id), row_type_(config.table_info.row_type),
  sample_row_(ClassRegistry<AbstractRow>::GetRegistry().CreateObject(
    row_type_)),
  oplog_(table_id, std::ceil(static_cast<float>(config.oplog_capacity)
      / GlobalContext::get_num_bg_threads()), sample_row_),
  process_storage_(config.process_cache_capacity),
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
  //TIMER_BEGIN(table_id_, GET);
  consistency_controller_->GetAsync(row_id);
  //TIMER_END(table_id_, GET);
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


void ClientTable::Get(int32_t row_id, RowAccessor *row_accessor) {
  TIMER_BEGIN(table_id_, GET);
  consistency_controller_->Get(row_id, row_accessor);
  TIMER_END(table_id_, GET);
}

void ClientTable::Inc(int32_t row_id, int32_t column_id, const void *update) {
  TIMER_BEGIN(table_id_, INC);
  consistency_controller_->Inc(row_id, column_id, update);
  TIMER_END(table_id_, INC);
}

void ClientTable::BatchInc(int32_t row_id, const int32_t* column_ids,
  const void* updates, int32_t num_updates) {
  TIMER_BEGIN(table_id_, BATCH_INC);
  consistency_controller_->BatchInc(row_id, column_ids, updates,
    num_updates);
  TIMER_END(table_id_, BATCH_INC);
}

void ClientTable::Clock() {
  consistency_controller_->Clock();
}

cuckoohash_map<int32_t, bool> *ClientTable::GetAndResetOpLogIndex(
    int32_t partition_num) {
  return oplog_index_.ResetPartition(partition_num);
}

}  // namespace petuum
