#include <petuum_ps/thread/value_table_oplog_meta_approx.hpp>

#include <petuum_ps/thread/context.hpp>

namespace petuum {
ValueTableOpLogMetaApprox::ValueTableOpLogMetaApprox(const AbstractRow *sample_row):
    RandomTableOpLogMeta(sample_row),
    uniform_dist_(0, 1) { }

ValueTableOpLogMetaApprox::~ValueTableOpLogMetaApprox() { }

void ValueTableOpLogMetaApprox::InsertMergeRowOpLogMeta(
    int32_t row_id,
    const RowOpLogMeta& row_oplog_meta) {
  auto iter = oplog_meta_.find(row_id);
  if (iter == oplog_meta_.end()) {
    oplog_meta_.insert(std::make_pair(row_id, row_oplog_meta));
    ++num_new_oplog_metas_;
    return;
  }
  iter->second.set_clock(row_oplog_meta.get_clock());
  iter->second.accum_importance(row_oplog_meta.get_importance());
}

void ValueTableOpLogMetaApprox::Prepare(size_t num_rows_to_send) {
  size_t oplog_meta_size = oplog_meta_.size();
  if (oplog_meta_size == 0) {
    sorted_vec_.resize(0);
    vec_iter_ = sorted_vec_.begin();
    return;
  }

  int32_t row_candidate_factor = GlobalContext::get_row_candidate_factor();

  size_t num_candidate_rows
      = num_rows_to_send*row_candidate_factor;
  if (num_candidate_rows > oplog_meta_size)
    num_candidate_rows = oplog_meta_size;

  sorted_vec_.resize(0);

  double select_prob = double(num_candidate_rows) / double(oplog_meta_size);

  for (const auto &meta_pair : oplog_meta_) {
    double prob = uniform_dist_(generator_);
    if (prob <= select_prob)
      sorted_vec_.push_back(meta_pair);
    if (sorted_vec_.size() == num_candidate_rows) break;
  }

  std::sort(sorted_vec_.begin(), sorted_vec_.end(),
            [] (const std::pair<int32_t, RowOpLogMeta> &oplog1,
                const std::pair<int32_t, RowOpLogMeta> &oplog2) {
              if (oplog1.second.get_importance()
                  == oplog2.second.get_importance()) {
                return oplog1.first < oplog2.first;
              } else {
                return (oplog1.second.get_importance()
                        > oplog2.second.get_importance());
              }
            });
  vec_iter_ = sorted_vec_.begin();
}

int32_t ValueTableOpLogMetaApprox::GetAndClearNextInOrder() {
  int32_t row_id = -1;
  std::unordered_map<int32_t, RowOpLogMeta>::iterator map_iter
      = oplog_meta_.end();

  do {
    if (vec_iter_ == sorted_vec_.end()) return -1;

    row_id = vec_iter_->first;
    map_iter = oplog_meta_.find(row_id);
    vec_iter_++;
  } while (map_iter == oplog_meta_.end());

  oplog_meta_.erase(map_iter);
  return row_id;
}
}
