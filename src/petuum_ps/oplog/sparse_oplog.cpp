#include <petuum_ps/oplog/sparse_oplog.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps_common/include/constants.hpp>
#include <petuum_ps_common/include/configs.hpp>
#include <petuum_ps/oplog/meta_row_oplog.hpp>
#include <petuum_ps_common/oplog/dense_row_oplog.hpp>
#include <petuum_ps_common/oplog/sparse_row_oplog.hpp>

#include <functional>

namespace petuum {

SparseOpLog::SparseOpLog(int capacity, const AbstractRow *sample_row,
                               size_t dense_row_oplog_capacity,
                               int32_t row_oplog_type):
  update_size_(sample_row->get_update_size()),
  locks_(GlobalContext::GetLockPoolSize(capacity)),
  oplog_map_(capacity * kCuckooExpansionFactor),
  sample_row_(sample_row),
  dense_row_oplog_capacity_(dense_row_oplog_capacity) {

  if (GlobalContext::get_consistency_model() == SSPAggr) {
    if (row_oplog_type == RowOpLogType::kDenseRowOpLog)
      CreateRowOpLog_ = CreateRowOpLog::CreateDenseMetaRowOpLog;
    else if (row_oplog_type == RowOpLogType::kSparseRowOpLog)
      CreateRowOpLog_ = CreateRowOpLog::CreateSparseMetaRowOpLog;
    else
      CreateRowOpLog_ = CreateRowOpLog::CreateSparseVectorMetaRowOpLog;
  } else {
    if (row_oplog_type == RowOpLogType::kDenseRowOpLog)
      CreateRowOpLog_ = CreateRowOpLog::CreateDenseRowOpLog;
    else if (row_oplog_type == RowOpLogType::kSparseRowOpLog)
      CreateRowOpLog_ = CreateRowOpLog::CreateSparseRowOpLog;
    else
      CreateRowOpLog_ = CreateRowOpLog::CreateSparseVectorRowOpLog;
  }
}

SparseOpLog::~SparseOpLog() {
  auto iter = oplog_map_.begin();
  for(; !iter.is_end(); iter++){
    delete iter->second;
  }
}

int32_t SparseOpLog::Inc(int32_t row_id, int32_t column_id, const void *delta) {
  locks_.Lock(row_id);
  AbstractRowOpLog *row_oplog = 0;
  if(!oplog_map_.find(row_id, row_oplog)){
    row_oplog = CreateRowOpLog_(update_size_, sample_row_,
                                dense_row_oplog_capacity_);
    oplog_map_.insert(row_id, row_oplog);
  }

  void *oplog_delta = row_oplog->FindCreate(column_id);
  sample_row_->AddUpdates(column_id, oplog_delta, delta);
  locks_.Unlock(row_id);

  return 0;
}

int32_t SparseOpLog::BatchInc(int32_t row_id, const int32_t *column_ids,
  const void *deltas, int32_t num_updates) {
  locks_.Lock(row_id);
  AbstractRowOpLog *row_oplog = 0;
  if(!oplog_map_.find(row_id, row_oplog)){
    row_oplog = CreateRowOpLog_(update_size_, sample_row_,
                                dense_row_oplog_capacity_);
    oplog_map_.insert(row_id, row_oplog);
  }
  const uint8_t* deltas_uint8 = reinterpret_cast<const uint8_t*>(deltas);

  for (int i = 0; i < num_updates; ++i) {
    void *oplog_delta = row_oplog->FindCreate(column_ids[i]);
    sample_row_->AddUpdates(column_ids[i], oplog_delta, deltas_uint8
      + update_size_*i);
  }
  locks_.Unlock(row_id);
  return 0;
}

int32_t SparseOpLog::DenseBatchInc(int32_t row_id, const void *updates,
                                   int32_t index_st, int32_t num_updates) {
  LOG(FATAL) << "Unsupported operation";
  return 0;
}

bool SparseOpLog::FindOpLog(int32_t row_id, OpLogAccessor *oplog_accessor) {
  locks_.Lock(row_id, oplog_accessor->get_unlock_ptr());
  AbstractRowOpLog *row_oplog;
  if (oplog_map_.find(row_id, row_oplog)) {
    oplog_accessor->set_row_oplog(row_oplog);
    return true;
  }
  (oplog_accessor->get_unlock_ptr())->Release();
  locks_.Unlock(row_id);
  return false;
}

bool SparseOpLog::FindInsertOpLog(int32_t row_id,
  OpLogAccessor *oplog_accessor) {
  bool new_create = false;
  locks_.Lock(row_id, oplog_accessor->get_unlock_ptr());
  AbstractRowOpLog *row_oplog;
  if (!oplog_map_.find(row_id, row_oplog)) {
    new_create = true;
    row_oplog = CreateRowOpLog_(update_size_, sample_row_,
                                dense_row_oplog_capacity_);
    oplog_map_.insert(row_id, row_oplog);
  }
  oplog_accessor->set_row_oplog(row_oplog);
  return new_create;
}

bool SparseOpLog::FindAndLock(int32_t row_id,
                                 OpLogAccessor *oplog_accessor) {
  locks_.Lock(row_id, oplog_accessor->get_unlock_ptr());
  AbstractRowOpLog *row_oplog;
  if (oplog_map_.find(row_id, row_oplog)) {
    oplog_accessor->set_row_oplog(row_oplog);
    return true;
  }
  return false;
}

AbstractRowOpLog *SparseOpLog::FindOpLog(int row_id) {
  AbstractRowOpLog *row_oplog;
  if (oplog_map_.find(row_id, row_oplog)) {
    return row_oplog;
  }
  return 0;
}

AbstractRowOpLog *SparseOpLog::FindInsertOpLog(int row_id) {
  AbstractRowOpLog *row_oplog;
  if (!oplog_map_.find(row_id, row_oplog)) {
    row_oplog = CreateRowOpLog_(update_size_, sample_row_,
                                dense_row_oplog_capacity_);
    oplog_map_.insert(row_id, row_oplog);
  }
  return row_oplog;
}

bool SparseOpLog::GetEraseOpLog(int32_t row_id,
                                AbstractRowOpLog **row_oplog_ptr) {
  Unlocker<> unlocker;
  locks_.Lock(row_id, &unlocker);
  AbstractRowOpLog *row_oplog = 0;
  if (!oplog_map_.find(row_id, row_oplog)) {
    return false;
  }
  oplog_map_.erase(row_id);
  *row_oplog_ptr = row_oplog;
  return true;
}

bool SparseOpLog::GetEraseOpLogIf(int32_t row_id,
                                  GetOpLogTestFunc test,
                                  void *test_args,
                                  AbstractRowOpLog **row_oplog_ptr) {
  Unlocker<> unlocker;
  locks_.Lock(row_id, &unlocker);
  AbstractRowOpLog *row_oplog = 0;
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

bool SparseOpLog::GetInvalidateOpLogMeta(int32_t row_id,
                                         RowOpLogMeta *row_oplog_meta) {
  Unlocker<> unlocker;
  locks_.Lock(row_id, &unlocker);
  AbstractRowOpLog *row_oplog = 0;
  if (!oplog_map_.find(row_id, row_oplog)) {
    return false;
  }

  MetaRowOpLog *meta_row_oplog = dynamic_cast<MetaRowOpLog*>(row_oplog);

  *row_oplog_meta = meta_row_oplog->GetMeta();
  meta_row_oplog->InvalidateMeta();
  meta_row_oplog->ResetImportance();
  return true;
}

AbstractAppendOnlyBuffer *SparseOpLog::GetAppendOnlyBuffer(int32_t comm_channel_idx) {
  LOG(FATAL) << "Unsupported operation for OpLogType";
  return 0;
}

void SparseOpLog::PutBackBuffer(int32_t comm_channel_idx, AbstractAppendOnlyBuffer *buff) {
  LOG(FATAL) << "Unsupported operation for OpLogType";
}

}
