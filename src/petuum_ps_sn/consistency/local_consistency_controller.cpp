
#include <petuum_ps_sn/consistency/local_consistency_controller.hpp>
#include <petuum_ps_common/storage/process_storage.hpp>
#include <petuum_ps_sn/thread/context.hpp>
#include <petuum_ps_common/util/stats.hpp>
#include <petuum_ps_common/util/class_register.hpp>
#include <glog/logging.h>
#include <algorithm>


namespace petuum {

LocalConsistencyController::LocalConsistencyController(
  const ClientTableConfig& config,
  int32_t table_id,
  ProcessStorage& process_storage,
  const AbstractRow* sample_row,
  boost::thread_specific_ptr<ThreadTableSN> &thread_cache) :
  AbstractConsistencyController(table_id, process_storage,
    sample_row),
  staleness_(config.table_info.table_staleness),
  row_type_(config.table_info.row_type),
  row_capacity_(config.table_info.row_capacity),
  locks_(GlobalContextSN::GetLockPoolSize(config.process_cache_capacity)),
  thread_cache_(thread_cache) { }

void LocalConsistencyController::CheckWaitClock(int32_t staleness) {
  if (ThreadContextSN::get_clock_ahead() >= -staleness)
    return;

  int32_t min_clock = GlobalContextSN::vector_clock_.get_min_clock();
  int32_t clock_ahead = min_clock - ThreadContextSN::get_clock();

  if (clock_ahead >= staleness) {
    ThreadContextSN::set_clock_ahead(clock_ahead);
    return;
  }

  std::unique_lock<std::mutex> lock(GlobalContextSN::clock_mtx_);
  min_clock = GlobalContextSN::vector_clock_.get_min_clock();
  clock_ahead = min_clock - ThreadContextSN::get_clock();

  if (clock_ahead >= -staleness) {
    ThreadContextSN::set_clock_ahead(clock_ahead);
    return;
  }

  while (clock_ahead < -staleness)  {
    GlobalContextSN::clock_cond_var_.wait(lock);
    min_clock = GlobalContextSN::vector_clock_.get_min_clock();
    clock_ahead = min_clock - ThreadContextSN::get_clock();
  }

  ThreadContextSN::set_clock_ahead(clock_ahead);
}

void LocalConsistencyController::CreateInsertRow(int32_t row_id,
                                                 RowAccessor *row_accessor) {
  Unlocker<> unlocker;
  locks_.Lock(row_id, &unlocker);
  bool found = process_storage_.Find(row_id, row_accessor);
  if (found)
    return;

  AbstractRow *row_data
    = ClassRegistry<AbstractRow>::GetRegistry().CreateObject(row_type_);
  row_data->Init(row_capacity_);
  ClientRow *client_row = new ClientRow(0, row_data);
  int32_t evicted_row_id = 0;
  process_storage_.Insert(row_id, client_row, row_accessor, &evicted_row_id);
  CHECK_EQ(evicted_row_id, -1)
    << "table id = " << table_id_
    << " to be inserted = " << row_id;
}

void LocalConsistencyController::Get(int32_t row_id, RowAccessor* row_accessor) {
  STATS_APP_SAMPLE_SSP_GET_BEGIN(table_id_);

  CheckWaitClock(staleness_);

  bool found = process_storage_.Find(row_id, row_accessor);
  if (found) {
    STATS_APP_SAMPLE_SSP_GET_END(table_id_, true);
    return;
  }

  CreateInsertRow(row_id, row_accessor);
  STATS_APP_SAMPLE_SSP_GET_END(table_id_, false);
}

void LocalConsistencyController::Inc(int32_t row_id, int32_t column_id,
    const void* delta) {

  RowAccessor row_accessor;
  bool found = process_storage_.Find(row_id, &row_accessor);
  if (!found)
    CreateInsertRow(row_id, &row_accessor);

  row_accessor.GetRowData()->ApplyInc(column_id, delta);
}

void LocalConsistencyController::BatchInc(int32_t row_id,
  const int32_t* column_ids, const void* updates, int32_t num_updates) {

  RowAccessor row_accessor;
  bool found = process_storage_.Find(row_id, &row_accessor);
  if (!found)
    CreateInsertRow(row_id, &row_accessor);

  row_accessor.GetRowData()->ApplyBatchInc(column_ids, updates,
					   num_updates);
}

void LocalConsistencyController::ThreadGet(int32_t row_id,
  ThreadRowAccessor* row_accessor) {
  STATS_APP_SAMPLE_THREAD_GET_BEGIN(table_id_);

  AbstractRow *row_data = thread_cache_->GetRow(row_id);
  if (row_data != 0) {
    row_accessor->row_data_ptr_ = row_data;
    STATS_APP_SAMPLE_THREAD_GET_END(table_id_);
    return;
  }

  CheckWaitClock(staleness_);

  RowAccessor process_row_accessor;
  bool found = process_storage_.Find(row_id, &process_row_accessor);
  if (!found)
    CreateInsertRow(row_id, &process_row_accessor);

  AbstractRow *tmp_row_data = process_row_accessor.GetRowData();
  thread_cache_->InsertRow(row_id, tmp_row_data);
  row_accessor->row_data_ptr_ = tmp_row_data;
  STATS_APP_SAMPLE_THREAD_GET_END(table_id_);
}

void LocalConsistencyController::ThreadInc(int32_t row_id, int32_t column_id,
    const void* delta) {
  thread_cache_->Inc(row_id, column_id, delta);
}

void LocalConsistencyController::ThreadBatchInc(int32_t row_id,
  const int32_t* column_ids, const void* updates, int32_t num_updates) {
  thread_cache_->BatchInc(row_id, column_ids, updates, num_updates);
}

void LocalConsistencyController::FlushThreadCache() {
  thread_cache_->FlushCache(process_storage_, sample_row_);
}

void LocalConsistencyController::Clock() {
  thread_cache_->FlushCache(process_storage_, sample_row_);
}

}   // namespace petuum
