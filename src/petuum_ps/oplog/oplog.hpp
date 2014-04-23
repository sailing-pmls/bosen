// Author: Jinliang

#pragma once

#include <boost/noncopyable.hpp>
#include <map>
#include <vector>
#include <mutex>
#include <utility>

#include "petuum_ps/util/lock.hpp"
#include "petuum_ps/oplog/oplog_partition.hpp"
#include "petuum_ps/thread/context.hpp"

namespace petuum {

// The OpLog storage for parameter server.  OpLogs are stored by table. Access
// to different tables are concurrent and lock-free.  Within each table,
// OpLogs are partitioned into N OpLogPartition (N is the number of bg
// threads). Accesses to different partitions are concurrent and lock-free;
// accesses to the same partition are guarded by a shared lock.  Accesses that
// may replace that OpLog partition with a new one are required to acquire the
// exclusive lock. Accesses that only modify or read the OpLogs are only
// required to acquire the shared lock.

// Relies on GlobalContext to be properly initialized.

// OpLogs for a particular table.
class TableOpLog : boost::noncopyable {
public:
  TableOpLog(int32_t table_id, int32_t partitioned_oplog_capacity,
    const AbstractRow *sample_row):
      table_id_(table_id),
      oplog_partitions_(GlobalContext::get_num_bg_threads()) {
      for (int32_t i = 0; i < GlobalContext::get_num_bg_threads(); ++i) {
        oplog_partitions_[i] = new OpLogPartition(partitioned_oplog_capacity,
          sample_row, table_id);
      }
    }

  ~TableOpLog() {
    for (int32_t i = 0; i < GlobalContext::get_num_bg_threads(); ++i) {
      delete oplog_partitions_[i];
    }
  }

  void Inc(int32_t row_id, int32_t column_id, const void *delta) {
    int32_t partition_num = GlobalContext::GetBgPartitionNum(row_id);
    oplog_partitions_[partition_num]->Inc(row_id, column_id, delta);
  }

  void BatchInc(int32_t row_id, const int32_t *column_ids, const void *deltas,
    int32_t num_updates) {
    int32_t partition_num = GlobalContext::GetBgPartitionNum(row_id);
    oplog_partitions_[partition_num]->BatchInc(row_id, column_ids, deltas,
      num_updates);
  }

  bool FindOpLog(int32_t row_id, OpLogAccessor *oplog_accessor) {
    int32_t partition_num = GlobalContext::GetBgPartitionNum(row_id);
    return oplog_partitions_[partition_num]->FindOpLog(row_id, oplog_accessor);
  }

  void FindInsertOpLog(int32_t row_id, OpLogAccessor *oplog_accessor) {
    int32_t partition_num = GlobalContext::GetBgPartitionNum(row_id);
    oplog_partitions_[partition_num]->FindInsertOpLog(row_id, oplog_accessor);
  }

  bool GetEraseOpLog(int32_t row_id, RowOpLog **row_oplog_ptr) {
    int32_t partition_num = GlobalContext::GetBgPartitionNum(row_id);
    return oplog_partitions_[partition_num]->GetEraseOpLog(row_id,
                                                          row_oplog_ptr);
  }

  bool GetEraseOpLogIf(int32_t row_id,
                       OpLogPartition::GetOpLogTestFunc test,
                       void *test_args, RowOpLog **row_oplog_ptr) {
    int32_t partition_num = GlobalContext::GetBgPartitionNum(row_id);
    return oplog_partitions_[partition_num]->GetEraseOpLogIf(row_id, test,
                                                            test_args,
                                                            row_oplog_ptr);
  }

private:
  int32_t table_id_;
  std::vector<OpLogPartition*> oplog_partitions_;

};

}   // namespace petuum
