#include "petuum_ps/client/thread_table.hpp"
#include "petuum_ps/thread/context.hpp"
#include "petuum_ps/include/row_access.hpp"

#include <glog/logging.h>

namespace petuum {

ThreadTable::ThreadTable(const AbstractRow *sample_row) :
    oplog_index_(GlobalContext::get_num_bg_threads()),
    sample_row_(sample_row){ }

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

void ThreadTable::IndexUpdate(int32_t row_id) {
  VLOG(0) << "oplog_index_.size() = " << oplog_index_.size();
  int32_t partition_num = GlobalContext::GetBgPartitionNum(row_id);
  VLOG(0) << "partition_num = " << partition_num;
  oplog_index_[partition_num][row_id] = true;
}

void ThreadTable::FlushOpLogIndex(TableOpLogIndex &table_oplog_index) {
  for (int32_t i = 0; i < GlobalContext::get_num_bg_threads(); ++i) {
    const boost::unordered_map<int32_t, bool> &partition_oplog_index
        = oplog_index_[i];
    table_oplog_index.AddIndex(i, partition_oplog_index);
    oplog_index_[i].clear();
  }
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

  boost::unordered_map<int32_t, RowOpLog* >::iterator oplog_iter
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

void ThreadTable::Inc(int32_t row_id, int32_t column_id, const void *delta) {
  boost::unordered_map<int32_t, RowOpLog* >::iterator oplog_iter
      = oplog_map_.find(row_id);

  RowOpLog *row_oplog;

  if (oplog_iter == oplog_map_.end()) {
    row_oplog = new RowOpLog(sample_row_->get_update_size(),
                                      sample_row_);
    oplog_map_[row_id] = row_oplog;
  } else {
    row_oplog = oplog_iter->second;
  }

  void *oplog_delta = row_oplog->FindCreate(column_id);
  sample_row_->AddUpdates(column_id, oplog_delta, delta);

  boost::unordered_map<int32_t, AbstractRow* >::iterator row_iter
      = row_storage_.find(row_id);
  if (row_iter != row_storage_.end()) {
    row_iter->second->ApplyIncUnsafe(row_id, delta);
  }
}

void ThreadTable::BatchInc(int32_t row_id, const int32_t *column_ids,
                           const void *deltas, int32_t num_updates) {
  boost::unordered_map<int32_t, RowOpLog* >::iterator oplog_iter
      = oplog_map_.find(row_id);

  RowOpLog *row_oplog;

  if (oplog_iter == oplog_map_.end()) {
    row_oplog = new RowOpLog(sample_row_->get_update_size(),
                                      sample_row_);
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

  boost::unordered_map<int32_t, AbstractRow* >::iterator row_iter
      = row_storage_.find(row_id);
  if (row_iter != row_storage_.end()) {
    row_iter->second->ApplyBatchIncUnsafe(column_ids, deltas, num_updates);
  }
}

void ThreadTable::FlushCache(ProcessStorage &process_storage,
                             TableOpLog &table_oplog) {
  for (auto oplog_iter = oplog_map_.begin(); oplog_iter != oplog_map_.end();
       oplog_iter++) {

    int32_t row_id = oplog_iter->first;
    int32_t partition_num = GlobalContext::GetBgPartitionNum(row_id);
    RowAccessor row_accessor;
    bool found = process_storage.Find(row_id, &row_accessor);

    int32_t column_id;
    void *delta = oplog_iter->second->BeginIterate(&column_id);
    while (delta != 0) {
      table_oplog.Inc(row_id, column_id, delta);
      oplog_index_[partition_num][row_id] = true;
      if (found) {
        row_accessor.GetRowData()->ApplyInc(column_id, delta);
      }
      delta = oplog_iter->second->Next(&column_id);
    }

    delete oplog_iter->second;
  }
  oplog_map_.clear();

  for (auto iter = row_storage_.begin(); iter != row_storage_.end(); iter++) {
    if (iter->second != 0)
      delete iter->second;
  }
  row_storage_.clear();
}

}
