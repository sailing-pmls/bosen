#include <petuum_ps/oplog/dense_oplog.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps_common/include/constants.hpp>
#include <petuum_ps_common/include/configs.hpp>
#include <petuum_ps/oplog/meta_row_oplog.hpp>
#include <petuum_ps_common/oplog/dense_row_oplog.hpp>
#include <petuum_ps_common/oplog/sparse_row_oplog.hpp>

#include <functional>

namespace petuum {

DenseOpLog::DenseOpLog(int capacity, const AbstractRow *sample_row,
                       size_t dense_row_oplog_capacity,
                       int32_t row_oplog_type,
                       bool version_maintain):
  update_size_(sample_row->get_update_size()),
  locks_(GlobalContext::GetLockPoolSize(capacity)),
  oplog_vec_(capacity, reinterpret_cast<AbstractRowOpLog*>(0)),
  sample_row_(sample_row),
  dense_row_oplog_capacity_(dense_row_oplog_capacity),
  capacity_(capacity) {
  if (GlobalContext::get_consistency_model() == SSPAggr) {
    if (row_oplog_type == RowOpLogType::kDenseRowOpLog) {
      if (!version_maintain)
        CreateRowOpLog_ = CreateRowOpLog::CreateDenseMetaRowOpLog;
      else
        CreateRowOpLog_ = CreateRowOpLog::CreateVersionDenseMetaRowOpLog;
    } else if (row_oplog_type == RowOpLogType::kSparseRowOpLog)
      CreateRowOpLog_ = CreateRowOpLog::CreateSparseMetaRowOpLog;
    else if(row_oplog_type == RowOpLogType::kDenseRowOpLogFloat16)
      CreateRowOpLog_ = CreateRowOpLog::CreateDenseMetaRowOpLogFloat16;
    else
      CreateRowOpLog_ = CreateRowOpLog::CreateSparseVectorMetaRowOpLog;
  } else {
    if (row_oplog_type == RowOpLogType::kDenseRowOpLog) {
      if (!version_maintain)
        CreateRowOpLog_ = CreateRowOpLog::CreateDenseRowOpLog;
      else
        CreateRowOpLog_ = CreateRowOpLog::CreateVersionDenseRowOpLog;
    } else if (row_oplog_type == RowOpLogType::kSparseRowOpLog)
      CreateRowOpLog_ = CreateRowOpLog::CreateSparseRowOpLog;
    else if(row_oplog_type == RowOpLogType::kDenseRowOpLogFloat16)
      CreateRowOpLog_ = CreateRowOpLog::CreateDenseRowOpLogFloat16;
    else
      CreateRowOpLog_ = CreateRowOpLog::CreateSparseVectorRowOpLog;
  }
}

DenseOpLog::~DenseOpLog() {
  for (auto &row : oplog_vec_) {
    if (row != 0) {
      delete row;
      row = 0;
    }
  }
}

int32_t DenseOpLog::Inc(int32_t row_id, int32_t column_id, const void *delta) {
  Unlocker<> unlocker;
  locks_.Lock(row_id, &unlocker);

  AbstractRowOpLog *row_oplog = FindRowOpLog(row_id);

  if (row_oplog == 0)
    row_oplog = CreateAndInsertRowOpLog(row_id);

  void *oplog_delta = row_oplog->FindCreate(column_id);
  sample_row_->AddUpdates(column_id, oplog_delta, delta);

  return 0;
}

int32_t DenseOpLog::BatchInc(int32_t row_id, const int32_t *column_ids,
                                   const void *deltas, int32_t num_updates) {
  Unlocker<> unlocker;
  locks_.Lock(row_id, &unlocker);

  AbstractRowOpLog *row_oplog = FindRowOpLog(row_id);

  if (row_oplog == 0)
    row_oplog = CreateAndInsertRowOpLog(row_id);

  const uint8_t* deltas_uint8 = reinterpret_cast<const uint8_t*>(deltas);

  for (int i = 0; i < num_updates; ++i) {
    void *oplog_delta = row_oplog->FindCreate(column_ids[i]);
    sample_row_->AddUpdates(column_ids[i], oplog_delta, deltas_uint8
                            + update_size_*i);
  }

  return 0;
}

int32_t DenseOpLog::DenseBatchInc(int32_t row_id, const void *updates,
                                   int32_t index_st, int32_t num_updates) {
  LOG(FATAL) << "Unsupported operation";
  return 0;
}

bool DenseOpLog::FindOpLog(int32_t row_id, OpLogAccessor *oplog_accessor) {
  locks_.Lock(row_id, oplog_accessor->get_unlock_ptr());

  AbstractRowOpLog *row_oplog = FindRowOpLog(row_id);
  if (row_oplog != 0) {
    oplog_accessor->set_row_oplog(row_oplog);
    return true;
  }

  (oplog_accessor->get_unlock_ptr())->Release();
  locks_.Unlock(row_id);
  return false;
}

bool DenseOpLog::FindInsertOpLog(int32_t row_id,
                                          OpLogAccessor *oplog_accessor) {
  bool new_create = false;
  locks_.Lock(row_id, oplog_accessor->get_unlock_ptr());

  AbstractRowOpLog *row_oplog = FindRowOpLog(row_id);

  if (row_oplog == 0) {
    row_oplog = CreateAndInsertRowOpLog(row_id);
    new_create = true;
  }

  oplog_accessor->set_row_oplog(row_oplog);
  return new_create;
}

bool DenseOpLog::FindAndLock(int32_t row_id,
                                 OpLogAccessor *oplog_accessor) {
  locks_.Lock(row_id, oplog_accessor->get_unlock_ptr());

  AbstractRowOpLog *row_oplog = FindRowOpLog(row_id);
  if (row_oplog != 0) {
    oplog_accessor->set_row_oplog(row_oplog);
    return true;
  }

  return false;
}

AbstractRowOpLog *DenseOpLog::FindOpLog(int row_id) {
  return FindRowOpLog(row_id);
}

AbstractRowOpLog *DenseOpLog::FindInsertOpLog(int row_id) {

  AbstractRowOpLog *row_oplog = FindRowOpLog(row_id);
  if (row_oplog == 0)
    row_oplog = CreateAndInsertRowOpLog(row_id);

  return row_oplog;
}

bool DenseOpLog::GetEraseOpLog(int32_t row_id,
                                   AbstractRowOpLog **row_oplog_ptr) {
  Unlocker<> unlocker;
  locks_.Lock(row_id, &unlocker);

  AbstractRowOpLog *row_oplog = FindRowOpLog(row_id);
  if (row_oplog == 0)
    return false;

  oplog_vec_[GetVecIndex(row_id)] = 0;
  *row_oplog_ptr = row_oplog;
  return true;
}

bool DenseOpLog::GetEraseOpLogIf(int32_t row_id,
                                 GetOpLogTestFunc test,
                                 void *test_args,
                                 AbstractRowOpLog **row_oplog_ptr) {
  Unlocker<> unlocker;
  locks_.Lock(row_id, &unlocker);

  AbstractRowOpLog *row_oplog = FindRowOpLog(row_id);
  if (row_oplog == 0)
    return false;

  if (!test(row_oplog, test_args)) {
    *row_oplog_ptr = 0;
    return true;
  }

  oplog_vec_[GetVecIndex(row_id)] = 0;
  *row_oplog_ptr = row_oplog;
  return true;
}

bool DenseOpLog::GetInvalidateOpLogMeta(int32_t row_id,
                                        RowOpLogMeta *row_oplog_meta) {
  Unlocker<> unlocker;
  locks_.Lock(row_id, &unlocker);

  AbstractRowOpLog *row_oplog = FindRowOpLog(row_id);
  if (row_oplog == 0)
    return false;

  MetaRowOpLog *meta_row_oplog = dynamic_cast<MetaRowOpLog*>(row_oplog);

  *row_oplog_meta = meta_row_oplog->GetMeta();
  meta_row_oplog->InvalidateMeta();
  meta_row_oplog->ResetImportance();
  return true;
}

AbstractAppendOnlyBuffer *DenseOpLog::GetAppendOnlyBuffer(int32_t comm_channel_idx) {
  LOG(FATAL) << "Unsupported operation for OpLogType";
  return 0;
}

void DenseOpLog::PutBackBuffer(int32_t comm_channel_idx, AbstractAppendOnlyBuffer *buff) {
  LOG(FATAL) << "Unsupported operation for OpLogType";
}

AbstractRowOpLog *DenseOpLog::FindRowOpLog(int32_t row_id) {
  int32_t vec_index = GetVecIndex(row_id);
  return oplog_vec_[vec_index];
}

AbstractRowOpLog *DenseOpLog::CreateAndInsertRowOpLog(int32_t row_id) {
  int32_t vec_index = GetVecIndex(row_id);
  AbstractRowOpLog* row_oplog = CreateRowOpLog_(update_size_, sample_row_,
                                                dense_row_oplog_capacity_);
  oplog_vec_[vec_index] = row_oplog;
  return row_oplog;
}

int32_t DenseOpLog::GetVecIndex(int32_t row_id) {
#ifdef PETUUM_CHECK_DENSE_STORAGE_RANGE
  CHECK_LT(row_id, capacity_);
#endif
  return row_id % capacity_;
}

}
