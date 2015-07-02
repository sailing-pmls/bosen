#include <petuum_ps/consistency/ssp_aggr_consistency_controller.hpp>
#include <petuum_ps/oplog/meta_row_oplog.hpp>

#include <glog/logging.h>

namespace petuum {

SSPAggrConsistencyController::SSPAggrConsistencyController(
    const TableInfo& info,
    int32_t table_id,
    AbstractProcessStorage& process_storage,
    AbstractOpLog& oplog,
    const AbstractRow* sample_row,
    boost::thread_specific_ptr<ThreadTable>& thread_cache,
    TableOpLogIndex& oplog_index,
    int32_t row_oplog_type):
    SSPPushConsistencyController(info, table_id, process_storage, oplog,
                                 sample_row, thread_cache, oplog_index, row_oplog_type) { }

void SSPAggrConsistencyController::Inc(int32_t row_id, int32_t column_id,
                                       const void *delta) {
  size_t thread_update_count
      = thread_cache_->IndexUpdateAndGetCount(row_id, 1);

  OpLogAccessor oplog_accessor;
  oplog_.FindInsertOpLog(row_id, &oplog_accessor);

  void *oplog_delta = oplog_accessor.get_row_oplog()->FindCreate(column_id);
  sample_row_->AddUpdates(column_id, oplog_delta, delta);

  MetaRowOpLog *meta_row_oplog
      = dynamic_cast<MetaRowOpLog*>(oplog_accessor.get_row_oplog());
  meta_row_oplog->GetMeta().set_clock(ThreadContext::get_clock());

  if (!version_maintain_) {
    RowAccessor row_accessor;
    ClientRow *client_row = process_storage_.Find(row_id, &row_accessor);
    if (client_row != 0) {
      client_row->GetRowDataPtr()->ApplyInc(column_id, delta);
    }
  }

  CheckAndFlushThreadCache(thread_update_count);
}

void SSPAggrConsistencyController::BatchInc(int32_t row_id,
  const int32_t *column_ids,
  const void *updates, int32_t num_updates) {

  size_t thread_update_count
      = thread_cache_->IndexUpdateAndGetCount(row_id, num_updates);

  OpLogAccessor oplog_accessor;
  oplog_.FindInsertOpLog(row_id, &oplog_accessor);

  const uint8_t* deltas_uint8 = reinterpret_cast<const uint8_t*>(updates);

  for (int i = 0; i < num_updates; ++i) {
    void *oplog_delta
        = oplog_accessor.get_row_oplog()->FindCreate(column_ids[i]);
    sample_row_->AddUpdates(column_ids[i], oplog_delta, deltas_uint8
			    + sample_row_->get_update_size()*i);
  }
  MetaRowOpLog *meta_row_oplog
      = dynamic_cast<MetaRowOpLog*>(oplog_accessor.get_row_oplog());
  meta_row_oplog->GetMeta().set_clock(ThreadContext::get_clock());

  if (!version_maintain_) {
    RowAccessor row_accessor;
    ClientRow *client_row = process_storage_.Find(row_id, &row_accessor);
    if (client_row != 0) {
      client_row->GetRowDataPtr()->ApplyBatchInc(column_ids, updates,
                                                 num_updates);
    }
  }
  CheckAndFlushThreadCache(thread_update_count);
}

void SSPAggrConsistencyController::DenseBatchInc(
    int32_t row_id, const void *updates, int32_t index_st,
    int32_t num_updates) {
  size_t thread_update_count
      = thread_cache_->IndexUpdateAndGetCount(row_id, num_updates);

  OpLogAccessor oplog_accessor;
  bool new_create = oplog_.FindInsertOpLog(row_id, &oplog_accessor);
  AbstractRowOpLog *row_oplog = oplog_accessor.get_row_oplog();

  if (new_create) {
    row_oplog->OverwriteWithDenseUpdate(updates, index_st, num_updates);
  } else {
    const uint8_t* deltas_uint8 = reinterpret_cast<const uint8_t*>(updates);
    (this->*DenseBatchIncOpLog_)(row_oplog, deltas_uint8,
                                 index_st, num_updates);
  }
  MetaRowOpLog *meta_row_oplog
      = dynamic_cast<MetaRowOpLog*>(row_oplog);
  meta_row_oplog->GetMeta().set_clock(ThreadContext::get_clock());

  if (!version_maintain_) {
    RowAccessor row_accessor;
    ClientRow *client_row = process_storage_.Find(row_id, &row_accessor);
    if (client_row != 0) {
      client_row->GetRowDataPtr()->ApplyDenseBatchInc(
          updates, index_st, num_updates);
    }
  }
  CheckAndFlushThreadCache(thread_update_count);
}

void SSPAggrConsistencyController::ThreadInc(int32_t row_id, int32_t column_id,
  const void *delta) {
  thread_cache_->Inc(row_id, column_id, delta);
  thread_cache_->AddToUpdateCount(1);
  size_t thread_update_count = thread_cache_->get_update_count();
  CheckAndFlushThreadCache(thread_update_count);
}

void SSPAggrConsistencyController::ThreadBatchInc(int32_t row_id,
  const int32_t *column_ids, const void *updates, int32_t num_updates) {

  thread_cache_->BatchInc(row_id, column_ids, updates, num_updates);
  thread_cache_->AddToUpdateCount(num_updates);
  size_t thread_update_count = thread_cache_->get_update_count();
  CheckAndFlushThreadCache(thread_update_count);
}

void SSPAggrConsistencyController::ThreadDenseBatchInc(
    int32_t row_id, const void *updates, int32_t index_st,
    int32_t num_updates) {
  thread_cache_->DenseBatchInc(row_id, updates, index_st, num_updates);
  thread_cache_->AddToUpdateCount(num_updates);
  size_t thread_update_count = thread_cache_->get_update_count();
  CheckAndFlushThreadCache(thread_update_count);
}

void SSPAggrConsistencyController::CheckAndFlushThreadCache(
    size_t thread_update_count) {

  if (thread_update_count > GlobalContext::get_thread_oplog_batch_size()) {
    thread_cache_->FlushCacheOpLog(process_storage_, oplog_, sample_row_);
    thread_cache_->FlushOpLogIndex(oplog_index_);

    thread_cache_->ResetUpdateCount();
  }
}

}
