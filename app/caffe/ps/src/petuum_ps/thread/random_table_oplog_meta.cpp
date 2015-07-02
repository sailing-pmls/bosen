#include <petuum_ps/thread/random_table_oplog_meta.hpp>

namespace petuum {
RandomTableOpLogMeta::RandomTableOpLogMeta(const AbstractRow *sample_row):
    sample_row_(sample_row),
    num_new_oplog_metas_(0),
    uniform_dist_(0, INT_MAX) { }

RandomTableOpLogMeta::~RandomTableOpLogMeta() { }

void RandomTableOpLogMeta::InsertMergeRowOpLogMeta(
    int32_t row_id, const RowOpLogMeta &row_oplog_meta) {
  auto iter = oplog_meta_.find(row_id);
  if (iter == oplog_meta_.end()) {
    oplog_meta_.insert(std::make_pair(row_id, row_oplog_meta));
    ++num_new_oplog_metas_;
    return;
  }
  iter->second.set_clock(row_oplog_meta.get_clock());
}

size_t RandomTableOpLogMeta::GetCleanNumNewOpLogMeta() {
  size_t tmp = num_new_oplog_metas_;
  num_new_oplog_metas_ = 0;
  return tmp;
}

int32_t RandomTableOpLogMeta::GetAndClearNextInOrder() {
  size_t oplog_meta_size = oplog_meta_.size();
  if (oplog_meta_size == 0) return -1;

  do {
    meta_iter_ = oplog_meta_.begin();
    std::advance(meta_iter_, uniform_dist_(generator_) % oplog_meta_size);
  } while(meta_iter_ == oplog_meta_.end());

  int32_t row_id = meta_iter_->first;

  oplog_meta_.erase(meta_iter_);
  return row_id;
}

int32_t RandomTableOpLogMeta::InitGetUptoClock(int32_t clock) {
  meta_iter_ = oplog_meta_.begin();
  clock_to_clear_ = clock;

  return GetAndClearNextUptoClock();
}

int32_t RandomTableOpLogMeta::GetAndClearNextUptoClock() {
  for (; meta_iter_ != oplog_meta_.end(); ++meta_iter_) {
    if (meta_iter_->second.get_clock() <= clock_to_clear_) {
      break;
    }
  }

  if (meta_iter_ == oplog_meta_.end()) return -1;

  int32_t row_id = meta_iter_->first;
  meta_iter_ = oplog_meta_.erase(meta_iter_);

  return row_id;
}

size_t RandomTableOpLogMeta::GetNumRowOpLogs() const {
  return oplog_meta_.size();
}

}
