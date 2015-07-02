#include <petuum_ps/consistency/ssp_aggr_value_consistency_controller.hpp>
#include <petuum_ps/oplog/meta_row_oplog.hpp>

#include <glog/logging.h>

namespace petuum {

SSPAggrValueConsistencyController::SSPAggrValueConsistencyController(
    const TableInfo& info,
    int32_t table_id,
    AbstractProcessStorage& process_storage,
    AbstractOpLog& oplog,
    const AbstractRow* sample_row,
    boost::thread_specific_ptr<ThreadTable>& thread_cache,
    TableOpLogIndex& oplog_index,
    int32_t row_oplog_type):
    SSPAggrConsistencyController(info, table_id, process_storage, oplog,
                                 sample_row, thread_cache, oplog_index,
                                 row_oplog_type) { }

void SSPAggrValueConsistencyController::Inc(int32_t row_id, int32_t column_id,
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

  double importance = 0.0;
  RowAccessor row_accessor;
  ClientRow *client_row = process_storage_.Find(row_id, &row_accessor);

  if (client_row != 0) {
    importance = client_row->GetRowDataPtr()->ApplyIncGetImportance(
        column_id, delta);
  } else {
    importance = sample_row_->GetImportance(column_id, delta);
  }
  meta_row_oplog->GetMeta().accum_importance(importance);
  CheckAndFlushThreadCache(thread_update_count);
}

void SSPAggrValueConsistencyController::BatchInc(int32_t row_id,
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

  double importance = 0.0;
  RowAccessor row_accessor;
  ClientRow *client_row = process_storage_.Find(row_id, &row_accessor);
  if (client_row != 0) {
    importance = client_row->GetRowDataPtr()->ApplyBatchIncGetImportance(
        column_ids, updates, num_updates);
  } else {
    importance = sample_row_->GetAccumImportance(
        column_ids, updates, num_updates);
  }
  meta_row_oplog->GetMeta().accum_importance(importance);
  CheckAndFlushThreadCache(thread_update_count);
}

void SSPAggrValueConsistencyController::DenseBatchInc(
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
    (this->*DenseBatchIncOpLog_)(row_oplog, deltas_uint8, index_st,
                                 num_updates);
  }
  MetaRowOpLog *meta_row_oplog
      = dynamic_cast<MetaRowOpLog*>(row_oplog);
  meta_row_oplog->GetMeta().set_clock(ThreadContext::get_clock());

  double importance = 0.0;
  RowAccessor row_accessor;
  ClientRow *client_row = process_storage_.Find(row_id, &row_accessor);
  if (client_row != 0) {
    if (!version_maintain_) {
      importance 
	= client_row->GetRowDataPtr()->ApplyDenseBatchIncGetImportance(
								       updates, index_st, num_updates);
    } else {
      importance 
	= client_row->GetRowDataPtr()->GetDenseAccumImportance(
							       updates, index_st, num_updates);
    }
  } else {
    importance 
      = sample_row_->GetDenseAccumImportance(
					     updates, index_st, num_updates);
  }
  meta_row_oplog->GetMeta().accum_importance(importance);
  CheckAndFlushThreadCache(thread_update_count);
}
}
