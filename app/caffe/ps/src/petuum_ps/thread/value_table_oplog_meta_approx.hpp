#pragma once

#include <petuum_ps/thread/random_table_oplog_meta.hpp>

#include <vector>
#include <unordered_map>
#include <random>

namespace petuum {

class ValueTableOpLogMetaApprox : public RandomTableOpLogMeta {
public:
  ValueTableOpLogMetaApprox(const AbstractRow *sample_row);
  virtual ~ValueTableOpLogMetaApprox();

  virtual void InsertMergeRowOpLogMeta(int32_t row_id,
                                       const RowOpLogMeta& row_oplog_meta);
  virtual void Prepare(size_t num_rows_to_send);
  virtual int32_t GetAndClearNextInOrder();

private:
  std::vector<std::pair<int32_t, RowOpLogMeta> > sorted_vec_;
  std::vector<std::pair<int32_t, RowOpLogMeta> >::iterator vec_iter_;

  std::mt19937 generator_; // max 4.2 billion
  std::uniform_real_distribution<> uniform_dist_;
};

}
