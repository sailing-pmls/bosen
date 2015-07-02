#pragma once

#include <petuum_ps/thread/abstract_table_oplog_meta.hpp>

#include <vector>
#include <unordered_map>
#include <random>

namespace petuum {

class FixedTableOpLogMetaDense : public AbstractTableOpLogMeta {
public:
  FixedTableOpLogMetaDense(const AbstractRow *sample_row, size_t table_size);
  virtual ~FixedTableOpLogMetaDense();

  void InsertMergeRowOpLogMeta(int32_t row_id,
                               const RowOpLogMeta& row_oplog_meta);

  virtual size_t GetCleanNumNewOpLogMeta();

  void Prepare(size_t num_rows_to_send);
  virtual int32_t GetAndClearNextInOrder();

  virtual int32_t InitGetUptoClock(int32_t clock);
  virtual int32_t GetAndClearNextUptoClock();

  virtual size_t GetNumRowOpLogs() const;

private:
  const AbstractRow *sample_row_;
  std::vector<RowOpLogMeta> meta_vec_;
  std::vector<RowOpLogMeta>::iterator meta_iter_;
  size_t num_valid_oplogs_;

  int32_t clock_to_clear_;

  size_t num_new_oplog_metas_;
};

}
