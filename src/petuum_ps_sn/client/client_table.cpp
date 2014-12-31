

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
