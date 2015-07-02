#include <petuum_ps/thread/random_table_oplog_meta_dense.hpp>

namespace petuum {
RandomTableOpLogMetaDense::RandomTableOpLogMetaDense(const AbstractRow *sample_row,
                                                     size_t table_size):
    sample_row_(sample_row),
    meta_vec_(table_size, RowOpLogMeta()),
    num_valid_oplogs_(0),
    uniform_dist_(0, 1),
    num_new_oplog_metas_(0) { }

RandomTableOpLogMetaDense::~RandomTableOpLogMetaDense() { }

void RandomTableOpLogMetaDense::InsertMergeRowOpLogMeta(
    int32_t row_id, const RowOpLogMeta &row_oplog_meta) {
  auto &my_row_oplog_meta = meta_vec_[row_id];
  if (my_row_oplog_meta.get_clock() == -1) {
    num_valid_oplogs_++;
    num_new_oplog_metas_++;
  }

  my_row_oplog_meta.set_clock(row_oplog_meta.get_clock());
}

size_t RandomTableOpLogMetaDense::GetCleanNumNewOpLogMeta() {
  size_t tmp = num_new_oplog_metas_;
  num_new_oplog_metas_ = 0;
  return tmp;
}

void RandomTableOpLogMetaDense::Prepare(size_t num_rows_to_send) {
  if (num_valid_oplogs_ == 0)
    return;
  size_t num_to_pick = num_rows_to_send;
  if (num_rows_to_send > num_valid_oplogs_) {
    num_to_pick = num_valid_oplogs_;
  }

  select_prob_ = double(num_to_pick) / double(num_valid_oplogs_);
  meta_iter_ = meta_vec_.begin();
}

int32_t RandomTableOpLogMetaDense::GetAndClearNextInOrder() {
  if (num_valid_oplogs_ == 0)
    return -1;

  while (meta_iter_ != meta_vec_.end()) {
    while (meta_iter_->get_clock() == -1
           && meta_iter_ != meta_vec_.end()) {
      meta_iter_++;
    }

    if (meta_iter_ == meta_vec_.end())
        return -1;

    double my_prob = uniform_dist_(generator_);

    if (my_prob <= select_prob_) {
      meta_iter_->invalidate_clock();
      num_valid_oplogs_--;
      return (meta_iter_ - meta_vec_.begin());
    }
    meta_iter_++;
  }
  return -1;
}

int32_t RandomTableOpLogMetaDense::InitGetUptoClock(int32_t clock) {
  meta_iter_ = meta_vec_.begin();
  clock_to_clear_ = clock;

  return GetAndClearNextUptoClock();
}

int32_t RandomTableOpLogMetaDense::GetAndClearNextUptoClock() {
  if (num_valid_oplogs_ == 0)
    return -1;

  for (; meta_iter_ != meta_vec_.end(); ++meta_iter_) {
    int32_t clock_to_check = meta_iter_->get_clock();
    if (clock_to_check >= 0
        && clock_to_check <= clock_to_clear_) {
      break;
    }
  }

  if (meta_iter_ == meta_vec_.end()) return -1;

  int32_t row_id = meta_iter_ - meta_vec_.begin();
  meta_iter_->invalidate_clock();
  num_valid_oplogs_--;

  return row_id;
}

size_t RandomTableOpLogMetaDense::GetNumRowOpLogs() const {
  return num_valid_oplogs_;
}

}
