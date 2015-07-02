#include <petuum_ps/thread/append_only_row_oplog_buffer.hpp>

namespace petuum {

void AppendOnlyRowOpLogBuffer::BatchIncGeneral(
    AbstractRowOpLog *row_oplog,
    const int32_t *col_ids, const void *updates, int32_t num_updates) {
  const uint8_t* deltas_uint8 = reinterpret_cast<const uint8_t*>(updates);
  for (int i = 0; i < num_updates; ++i) {
    void *oplog_delta = row_oplog->FindCreate(col_ids[i]);
    sample_row_->AddUpdates(col_ids[i], oplog_delta,
                            deltas_uint8 + update_size_*i);
  }
}

void AppendOnlyRowOpLogBuffer::BatchIncDense(
    AbstractRowOpLog *row_oplog,
    const int32_t *col_ids, const void *updates, int32_t num_updates) {
  const uint8_t* deltas_uint8 = reinterpret_cast<const uint8_t*>(updates);

  for (int i = 0; i < num_updates; ++i) {
    void *oplog_delta = row_oplog->FindCreate(i);
    sample_row_->AddUpdates(i, oplog_delta, deltas_uint8 + update_size_*i);
  }
}

void AppendOnlyRowOpLogBuffer::BatchInc(
    int32_t row_id, const int32_t *col_ids,
    const void *updates, int32_t num_updates) {
  auto iter = row_oplog_map_.find(row_id);
  if (iter == row_oplog_map_.end()) {
    AbstractRowOpLog *row_oplog = row_oplog_generator_.GetRowOpLog();
    row_oplog_map_.insert(std::make_pair(row_id, row_oplog));
    iter = row_oplog_map_.find(row_id);
  }
  (this->*BatchInc_)(iter->second, col_ids, updates, num_updates);
}

void AppendOnlyRowOpLogBuffer::BatchIncTmp(
    int32_t row_id, const int32_t *col_ids,
    const void *updates, int32_t num_updates) {
  auto iter = tmp_row_oplog_map_.find(row_id);
  if (iter == tmp_row_oplog_map_.end()) {
    AbstractRowOpLog *row_oplog = row_oplog_generator_.GetRowOpLog();
    tmp_row_oplog_map_.insert(std::make_pair(row_id, row_oplog));
    iter = tmp_row_oplog_map_.find(row_id);
  }
  //LOG(INFO) << "BatchInc_ = " << BatchInc_;
  (this->*BatchInc_)(iter->second, col_ids, updates, num_updates);
}

void AppendOnlyRowOpLogBuffer::MergeTmpOpLog() {
  for (auto &tmp_row_oplog_iter : tmp_row_oplog_map_) {
    int32_t row_id = tmp_row_oplog_iter.first;
    AbstractRowOpLog *tmp_row_oplog = tmp_row_oplog_iter.second;

    auto row_oplog_iter = row_oplog_map_.find(row_id);
    if (row_oplog_iter == row_oplog_map_.end()) {
      row_oplog_map_.insert(std::make_pair(row_id, tmp_row_oplog));
    } else {
      int32_t col_id = 0;
      const void *update = tmp_row_oplog->BeginIterateConst(&col_id);
      while (update != 0) {
        void *oplog = row_oplog_iter->second->FindCreate(col_id);
        sample_row_->AddUpdates(col_id, oplog, update);
        update = tmp_row_oplog->NextConst(&col_id);
      }
      row_oplog_generator_.PutBackRowOpLog(tmp_row_oplog);
    }
  }
  tmp_row_oplog_map_.clear();
}

AbstractRowOpLog *AppendOnlyRowOpLogBuffer::InitReadTmpOpLog(int32_t *row_id) {
  map_iter_ = tmp_row_oplog_map_.begin();

  if (map_iter_ == tmp_row_oplog_map_.end())
    return 0;

  *row_id = map_iter_->first;
  return map_iter_->second;
}

AbstractRowOpLog *AppendOnlyRowOpLogBuffer::NextReadTmpOpLog(int32_t *row_id) {
  ++map_iter_;
  if (map_iter_ == tmp_row_oplog_map_.end())
    return 0;

  *row_id = map_iter_->first;
  return map_iter_->second;
}

AbstractRowOpLog *AppendOnlyRowOpLogBuffer::InitReadRmOpLog(int32_t *row_id) {
  map_iter_ = row_oplog_map_.begin();

  if (map_iter_ == row_oplog_map_.end())
    return 0;

  *row_id = map_iter_->first;
  AbstractRowOpLog *row_oplog = map_iter_->second;

  std::unordered_map<int32_t, AbstractRowOpLog*>::iterator tmp_iter
      = row_oplog_map_.erase(map_iter_);

  map_iter_ = tmp_iter;

  return row_oplog;
}

AbstractRowOpLog *AppendOnlyRowOpLogBuffer::NextReadRmOpLog(int32_t *row_id) {
  if (map_iter_ == row_oplog_map_.end())
    return 0;

  *row_id = map_iter_->first;
  AbstractRowOpLog *row_oplog = map_iter_->second;

  std::unordered_map<int32_t, AbstractRowOpLog*>::iterator tmp_iter
      = row_oplog_map_.erase(map_iter_);

  map_iter_ = tmp_iter;
  return row_oplog;
}

AbstractRowOpLog *AppendOnlyRowOpLogBuffer::InitReadOpLog(int32_t *row_id) {
  map_iter_ = row_oplog_map_.begin();

  if (map_iter_ == row_oplog_map_.end())
    return 0;

  *row_id = map_iter_->first;
  return map_iter_->second;
}

AbstractRowOpLog *AppendOnlyRowOpLogBuffer::NextReadOpLog(int32_t *row_id) {
  ++map_iter_;
  if (map_iter_ == row_oplog_map_.end())
    return 0;

  *row_id = map_iter_->first;
  return map_iter_->second;
}

AbstractRowOpLog *AppendOnlyRowOpLogBuffer::GetRowOpLog(int32_t row_id) const {
  auto map_iter = row_oplog_map_.find(row_id);

  if (map_iter == row_oplog_map_.end())
    return 0;

  return map_iter->second;
}

}
