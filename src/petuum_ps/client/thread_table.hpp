#pragma once

#include <boost/unordered_map.hpp>
#include <vector>
#include <boost/noncopyable.hpp>

#include "petuum_ps/include/abstract_row.hpp"
#include "petuum_ps/oplog/oplog_index.hpp"
#include "petuum_ps/oplog/oplog.hpp"
#include "petuum_ps/storage/process_storage.hpp"

namespace petuum {

class ThreadTable : boost::noncopyable {
public:
  explicit ThreadTable(const AbstractRow *sample_row);
  ~ThreadTable();
  void IndexUpdate(int32_t row_id);
  void FlushOpLogIndex(TableOpLogIndex &oplog_index);

  AbstractRow *GetRow(int32_t row_id);
  void InsertRow(int32_t row_id, const AbstractRow *to_insert);
  void Inc(int32_t row_id, int32_t column_id, const void *delta);
  void BatchInc(int32_t row_id, const int32_t *column_ids,
    const void *deltas, int32_t num_updates);

  void FlushCache(ProcessStorage &process_storage, TableOpLog &table_oplog);

private:
  std::vector<boost::unordered_map<int32_t, bool> > oplog_index_;
  boost::unordered_map<int32_t, AbstractRow* > row_storage_;
  boost::unordered_map<int32_t, RowOpLog* > oplog_map_;
  const AbstractRow *sample_row_;
};

}
