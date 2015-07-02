#pragma once

#include <unordered_set>
#include <vector>
#include <boost/noncopyable.hpp>
#include <boost/unordered_map.hpp>

#include <petuum_ps_common/include/abstract_row.hpp>
#include <petuum_ps_common/storage/process_storage.hpp>
#include <petuum_ps_common/oplog/row_oplog.hpp>

namespace petuum {

class ThreadTableSN : boost::noncopyable {
public:
  explicit ThreadTableSN(const AbstractRow *sample_row);
  ~ThreadTableSN();

  AbstractRow *GetRow(int32_t row_id);
  void InsertRow(int32_t row_id, const AbstractRow *to_insert);
  void Inc(int32_t row_id, int32_t column_id, const void *delta);
  void BatchInc(int32_t row_id, const int32_t *column_ids,
    const void *deltas, int32_t num_updates);

  void FlushCache(ProcessStorage &process_storage,
		  const AbstractRow *sample_row);

private:
  boost::unordered_map<int32_t, AbstractRow* > row_storage_;
  boost::unordered_map<int32_t, RowOpLog* > oplog_map_;
  const AbstractRow * const sample_row_;
};

}
