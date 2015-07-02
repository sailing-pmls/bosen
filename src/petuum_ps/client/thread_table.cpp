#include <petuum_ps/client/thread_table.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps_common/include/row_access.hpp>
#include <petuum_ps/oplog/meta_row_oplog.hpp>
#include <petuum_ps_common/include/configs.hpp>
#include <petuum_ps/oplog/create_row_oplog.hpp>

#include <glog/logging.h>
#include <functional>

namespace petuum {

ThreadTable::ThreadTable(
    const AbstractRow *sample_row, int32_t row_oplog_type,
    size_t dense_row_oplog_capacity) :
    oplog_index_(GlobalContext::get_num_comm_channels_per_client()),
    sample_row_(sample_row),
    update_count_(0),
    dense_row_oplog_capacity_(dense_row_oplog_capacity) {

  ApplyThreadOpLog_ = &ThreadTable::ApplyThreadOpLogSSP;

  if (GlobalContext::get_consistency_model() == SSPAggr) {
    UpdateOpLogClock_ = UpdateOpLogClockSSPAggr;

    if (row_oplog_type == RowOpLogType::kDenseRowOpLog) {
      CreateRowOpLog_ = CreateRowOpLog::CreateDenseMetaRowOpLog;
    } else if (row_oplog_type == RowOpLogType::kSparseRowOpLog) {
      CreateRowOpLog_ = CreateRowOpLog::CreateSparseMetaRowOpLog;
    } else {
      CreateRowOpLog_ = CreateRowOpLog::CreateSparseVectorMetaRowOpLog;
    }

    if (GlobalContext::get_update_sort_policy() == RelativeMagnitude
        || GlobalContext::get_update_sort_policy() == FIFO_N_ReMag) {
      ApplyThreadOpLog_ = &ThreadTable::ApplyThreadOpLogGetImportance;
    }
  } else {
    UpdateOpLogClock_ = UpdateOpLogClockNoOp;
    if (row_oplog_type == RowOpLogType::kDenseRowOpLog)
      CreateRowOpLog_ = CreateRowOpLog::CreateDenseRowOpLog;
    else if (row_oplog_type == RowOpLogType::kSparseRowOpLog)
      CreateRowOpLog_ = CreateRowOpLog::CreateSparseRowOpLog;
    else
      CreateRowOpLog_ = CreateRowOpLog::CreateSparseVectorRowOpLog;
  }
}

ThreadTable::~ThreadTable() {
  for (auto iter = row_storage_.begin(); iter != row_storage_.end(); iter++) {
    if (iter->second != 0)
      delete iter->second;
  }

  for (auto iter = oplog_map_.begin(); iter != oplog_map_.end(); iter++) {
    if (iter->second != 0)
      delete iter->second;
  }
}

void ThreadTable::UpdateOpLogClockSSPAggr(AbstractRowOpLog *row_oplog) {
  MetaRowOpLog *meta_row_oplog
      = dynamic_cast<MetaRowOpLog*>(row_oplog);
  meta_row_oplog->GetMeta().set_clock(ThreadContext::get_clock());
}

void ThreadTable::IndexUpdate(int32_t row_id) {
  int32_t partition_num = GlobalContext::GetPartitionCommChannelIndex(row_id);
  oplog_index_[partition_num].insert(row_id);
}

size_t ThreadTable::IndexUpdateAndGetCount(int32_t row_id, size_t num_updates) {
  int32_t partition_num = GlobalContext::GetPartitionCommChannelIndex(row_id);
  oplog_index_[partition_num].insert(row_id);
  update_count_ += num_updates;
  return update_count_;
}

void ThreadTable::ResetUpdateCount() {
  update_count_ = 0;
}

void ThreadTable::AddToUpdateCount(size_t num_updates) {
  update_count_ += num_updates;
}

void ThreadTable::FlushOpLogIndex(TableOpLogIndex &table_oplog_index) {
  for (int32_t i = 0; i < GlobalContext::get_num_comm_channels_per_client();
       ++i) {
    const std::unordered_set<int32_t> &partition_oplog_index
        = oplog_index_[i];
    table_oplog_index.AddIndex(i, partition_oplog_index);
    oplog_index_[i].clear();
  }
  ResetUpdateCount();
}

AbstractRow *ThreadTable::GetRow(int32_t row_id) {
  boost::unordered_map<int32_t, AbstractRow* >::iterator row_iter
      = row_storage_.find(row_id);
  if (row_iter == row_storage_.end()) {
    return 0;
  }
  return row_iter->second;
}

void ThreadTable::InsertRow(int32_t row_id, const AbstractRow *to_insert) {
  AbstractRow *row = to_insert->Clone();
  boost::unordered_map<int32_t, AbstractRow* >::iterator row_iter
      = row_storage_.find(row_id);
  if (row_iter != row_storage_.end()) {
    delete row_iter->second;
    row_iter->second = row;
  } else {
    row_storage_[row_id] = row;
  }

  boost::unordered_map<int32_t, AbstractRowOpLog* >::iterator oplog_iter
      = oplog_map_.find(row_id);
  if (oplog_iter != oplog_map_.end()) {
    int32_t column_id;
    void *delta = oplog_iter->second->BeginIterate(&column_id);
    while (delta != 0) {
      row->ApplyInc(column_id, delta);
      delta = oplog_iter->second->Next(&column_id);
    }
  }
}

// The assumption is that thread oplog will be flushed every clock, so we only
// need to

void ThreadTable::Inc(int32_t row_id, int32_t column_id, const void *delta) {
  auto oplog_iter = oplog_map_.find(row_id);

  AbstractRowOpLog *row_oplog;
  if (oplog_iter == oplog_map_.end()) {
    row_oplog = CreateRowOpLog_(sample_row_->get_update_size(), sample_row_,
                                dense_row_oplog_capacity_);
    oplog_map_[row_id] = row_oplog;
  } else {
    row_oplog = oplog_iter->second;
  }

  void *oplog_delta = row_oplog->FindCreate(column_id);
  sample_row_->AddUpdates(column_id, oplog_delta, delta);

  auto row_iter = row_storage_.find(row_id);
  if (row_iter != row_storage_.end()) {
    row_iter->second->ApplyIncUnsafe(column_id, delta);
  }
}

void ThreadTable::BatchInc(int32_t row_id, const int32_t *column_ids,
                           const void *deltas, int32_t num_updates) {
  auto oplog_iter = oplog_map_.find(row_id);

  AbstractRowOpLog *row_oplog;
  if (oplog_iter == oplog_map_.end()) {
    row_oplog = CreateRowOpLog_(sample_row_->get_update_size(), sample_row_,
                                dense_row_oplog_capacity_);
    oplog_map_[row_id] = row_oplog;
  } else {
    row_oplog = oplog_iter->second;
  }

  const uint8_t* deltas_uint8 = reinterpret_cast<const uint8_t*>(deltas);

  for (int i = 0; i < num_updates; ++i) {
    void *oplog_delta = row_oplog->FindCreate(column_ids[i]);
    sample_row_->AddUpdates(column_ids[i], oplog_delta, deltas_uint8
                            + sample_row_->get_update_size()*i);
  }

  auto row_iter = row_storage_.find(row_id);
  if (row_iter != row_storage_.end()) {
    row_iter->second->ApplyBatchIncUnsafe(column_ids, deltas, num_updates);
  }
}

void ThreadTable::DenseBatchInc(int32_t row_id, const void *updates,
                                int32_t index_st, int32_t num_updates) {
  auto oplog_iter = oplog_map_.find(row_id);
  AbstractRowOpLog *row_oplog;
  if (oplog_iter == oplog_map_.end()) {
    row_oplog = CreateRowOpLog_(sample_row_->get_update_size(), sample_row_,
                                dense_row_oplog_capacity_);
    oplog_map_[row_id] = row_oplog;
    row_oplog->OverwriteWithDenseUpdate(updates, index_st, num_updates);
  } else {
    row_oplog = oplog_iter->second;
    const uint8_t* updates_uint8 = reinterpret_cast<const uint8_t*>(updates);
    for (int i = 0; i < num_updates; ++i) {
      int32_t col_id = i + index_st;
      void *oplog_update = row_oplog->FindCreate(col_id);
      sample_row_->AddUpdates(col_id, oplog_update, updates_uint8
                              + sample_row_->get_update_size()*i);
    }
  }

  auto row_iter = row_storage_.find(row_id);
  if (row_iter != row_storage_.end()) {
    row_iter->second->ApplyDenseBatchIncUnsafe(updates, index_st, num_updates);
  }
}

void ThreadTable::FlushCache(AbstractProcessStorage &process_storage,
                             AbstractOpLog &table_oplog,
			     const AbstractRow *sample_row) {
  FlushCacheOpLog(process_storage, table_oplog, sample_row);

  for (auto iter = row_storage_.begin(); iter != row_storage_.end(); iter++) {
    if (iter->second != 0) {
      delete iter->second;
    }
  }
  row_storage_.clear();
}

void ThreadTable::FlushCacheOpLog(AbstractProcessStorage &process_storage,
                                  AbstractOpLog &table_oplog,
                                  const AbstractRow *sample_row) {
  for (auto oplog_iter = oplog_map_.begin(); oplog_iter != oplog_map_.end();
       oplog_iter++) {
    int32_t row_id = oplog_iter->first;

    OpLogAccessor oplog_accessor;
    table_oplog.FindInsertOpLog(row_id, &oplog_accessor);
    UpdateOpLogClock_(oplog_accessor.get_row_oplog());

    RowAccessor row_accessor;
    bool found = process_storage.Find(row_id, &row_accessor);

    (this->*ApplyThreadOpLog_)(&oplog_accessor, &row_accessor, found,
                               oplog_iter->second, row_id);
    delete oplog_iter->second;
  }
  oplog_map_.clear();
}

void ThreadTable::ApplyThreadOpLogSSP(
    OpLogAccessor *oplog_accessor, RowAccessor *row_accessor, bool row_found,
    AbstractRowOpLog *row_oplog, int32_t row_id) {

  int32_t partition_num = GlobalContext::GetPartitionCommChannelIndex(row_id);

  int32_t column_id;
  void *delta = row_oplog->BeginIterate(&column_id);
  while (delta != 0) {
    void *oplog_delta = oplog_accessor->get_row_oplog()->FindCreate(column_id);
    sample_row_->AddUpdates(column_id, oplog_delta, delta);

    oplog_index_[partition_num].insert(row_id);
    if (row_found) {
      row_accessor->GetRowData()->ApplyInc(column_id, delta);
    }

    delta = row_oplog->Next(&column_id);
  }
}

void ThreadTable::ApplyThreadOpLogGetImportance(
    OpLogAccessor *oplog_accessor, RowAccessor *row_accessor, bool row_found,
    AbstractRowOpLog *row_oplog, int32_t row_id) {

  int32_t partition_num = GlobalContext::GetPartitionCommChannelIndex(row_id);

  int32_t column_id;
  void *delta = row_oplog->BeginIterate(&column_id);
  double importance = 0.0;
  while (delta != 0) {
    void *oplog_delta = oplog_accessor->get_row_oplog()->FindCreate(column_id);
    sample_row_->AddUpdates(column_id, oplog_delta, delta);

    oplog_index_[partition_num].insert(row_id);
    if (row_found) {
      importance += row_accessor->GetRowData()->ApplyIncGetImportance(
          column_id, delta);
    } else {
      importance += sample_row_->GetImportance(column_id, delta);
    }
    delta = row_oplog->Next(&column_id);
  }

  MetaRowOpLog *meta_row_oplog
      = dynamic_cast<MetaRowOpLog*>(oplog_accessor->get_row_oplog());
  meta_row_oplog->GetMeta().accum_importance(importance);
}
}
