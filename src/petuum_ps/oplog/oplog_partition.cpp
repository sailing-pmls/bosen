#include "petuum_ps/oplog/oplog_partition.hpp"
#include "petuum_ps/thread/context.hpp"

namespace petuum {

OpLogPartition::OpLogPartition(int capacity, const AbstractRow *sample_row,
                               int32_t table_id):
  update_size_(sample_row->get_update_size()),
  locks_(GlobalContext::get_lock_pool_size()),
  oplog_map_(capacity * GlobalContext::get_cuckoo_expansion_factor()),
  sample_row_(sample_row),
  table_id_(table_id) { }

OpLogPartition::~OpLogPartition() {
  cuckoohash_map<int32_t, RowOpLog* >::iterator iter = oplog_map_.begin();
  for(; !iter.is_end(); iter++){
    delete iter->second;
  }
}

void OpLogPartition::Inc(int32_t row_id, int32_t column_id, const void *delta) {
  locks_.Lock(row_id);
  RowOpLog *row_oplog = 0;
  if(!oplog_map_.find(row_id, row_oplog)){
    row_oplog = new RowOpLog(update_size_, sample_row_);
    oplog_map_.insert(row_id, row_oplog);
  }

  void *oplog_delta = row_oplog->FindCreate(column_id);
  sample_row_->AddUpdates(column_id, oplog_delta, delta);
  locks_.Unlock(row_id);
}

void OpLogPartition::BatchInc(int32_t row_id, const int32_t *column_ids,
  const void *deltas, int32_t num_updates) {
  locks_.Lock(row_id);
  RowOpLog *row_oplog = 0;
  if(!oplog_map_.find(row_id, row_oplog)){
    row_oplog = new RowOpLog(update_size_, sample_row_);
    oplog_map_.insert(row_id, row_oplog);
  }

  const uint8_t* deltas_uint8 = reinterpret_cast<const uint8_t*>(deltas);

  for (int i = 0; i < num_updates; ++i) {
    void *oplog_delta = row_oplog->FindCreate(column_ids[i]);
    sample_row_->AddUpdates(column_ids[i], oplog_delta, deltas_uint8
      + update_size_*i);
  }
  locks_.Unlock(row_id);
}

bool OpLogPartition::FindOpLog(int32_t row_id, OpLogAccessor *oplog_accessor) {
  locks_.Lock(row_id, oplog_accessor->get_unlock_ptr());
  RowOpLog *row_oplog;
  if (oplog_map_.find(row_id, row_oplog)) {
    oplog_accessor->set_row_oplog(row_oplog);
    return true;
  }
  (oplog_accessor->get_unlock_ptr())->Release();
  locks_.Unlock(row_id);
  return false;
}

void OpLogPartition::FindInsertOpLog(int32_t row_id,
  OpLogAccessor *oplog_accessor) {
  locks_.Lock(row_id, oplog_accessor->get_unlock_ptr());
  RowOpLog *row_oplog;
  if (!oplog_map_.find(row_id, row_oplog)) {
    row_oplog = new RowOpLog(update_size_, sample_row_);
    oplog_map_.insert(row_id, row_oplog);
  }
  oplog_accessor->set_row_oplog(row_oplog);
}

RowOpLog *OpLogPartition::FindOpLog(int row_id) {
  RowOpLog *row_oplog;
  if (oplog_map_.find(row_id, row_oplog)) {
    return row_oplog;
  }
  return 0;
}

RowOpLog *OpLogPartition::FindInsertOpLog(int row_id) {
  RowOpLog *row_oplog;
  if (!oplog_map_.find(row_id, row_oplog)) {
    row_oplog = new RowOpLog(update_size_, sample_row_);
    oplog_map_.insert(row_id, row_oplog);
  }
  return row_oplog;
}

bool OpLogPartition::GetEraseOpLog(int32_t row_id,
                                   RowOpLog **row_oplog_ptr) {
  Unlocker<> unlocker;
  locks_.Lock(row_id, &unlocker);
  RowOpLog *row_oplog = 0;
  if (!oplog_map_.find(row_id, row_oplog)) {
    return false;
  }
  oplog_map_.erase(row_id);
  *row_oplog_ptr = row_oplog;
  return true;
}

bool OpLogPartition::GetEraseOpLogIf(int32_t row_id, GetOpLogTestFunc test,
                                     void *test_args,
                                     RowOpLog **row_oplog_ptr) {
  Unlocker<> unlocker;
  locks_.Lock(row_id, &unlocker);
  RowOpLog *row_oplog = 0;
  if (!oplog_map_.find(row_id, row_oplog)) {
    return false;
  }

  if (!test(row_oplog, test_args)) {
    *row_oplog_ptr = 0;
    return true;
  }

  oplog_map_.erase(row_id);
  *row_oplog_ptr = row_oplog;
  return true;
}

}
