// Author: Jinliang

#pragma once

#include <boost/noncopyable.hpp>
#include <map>
#include <vector>
#include <mutex>
#include <utility>

#include <petuum_ps_common/util/lock.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps/oplog/abstract_oplog.hpp>
#include <petuum_ps/oplog/append_only_oplog_partition.hpp>

namespace petuum {

// Relies on GlobalContext to be properly initialized.

// OpLogs for a particular table.
class AppendOnlyOpLog : public AbstractOpLog {
public:
  AppendOnlyOpLog(size_t append_only_buff_capacity,
                  const AbstractRow *sample_row,
                  AppendOnlyOpLogType append_only_oplog_type,
                  size_t dense_row_oplog_capacity,
                  size_t append_only_per_thread_buff_pool_size):
      oplog_partitions_(GlobalContext::get_num_comm_channels_per_client()) {
    for (int32_t i = 0; i < GlobalContext::get_num_comm_channels_per_client();
         ++i) {
      oplog_partitions_[i] = new AppendOnlyOpLogPartition(
          append_only_buff_capacity, sample_row,
          append_only_oplog_type,
          dense_row_oplog_capacity,
          append_only_per_thread_buff_pool_size);
    }
  }

  ~AppendOnlyOpLog() {
    for (int32_t i = 0; i < GlobalContext::get_num_comm_channels_per_client();
         ++i) {
      delete oplog_partitions_[i];
    }
  }

  void RegisterThread() {
    for (auto oplog_partition_ptr : oplog_partitions_) {
      oplog_partition_ptr->RegisterThread();
    }
  }

  void DeregisterThread() {
    for (auto oplog_partition_ptr : oplog_partitions_) {
      oplog_partition_ptr->DeregisterThread();
    }
  }

  int32_t Inc(int32_t row_id, int32_t column_id, const void *delta) {
    int32_t partition_num = GlobalContext::GetPartitionCommChannelIndex(row_id);
    int32_t buff_pushed
        = oplog_partitions_[partition_num]->Inc(row_id, column_id, delta);
    return (buff_pushed == 1) ? partition_num : -1;
  }

  int32_t BatchInc(int32_t row_id, const int32_t *column_ids, const void *deltas,
    int32_t num_updates) {
    int32_t partition_num = GlobalContext::GetPartitionCommChannelIndex(row_id);
    int32_t buff_pushed = oplog_partitions_[partition_num]->BatchInc(
        row_id, column_ids, deltas, num_updates);
    return (buff_pushed == 1) ? partition_num : -1;
  }

  int32_t DenseBatchInc(int32_t row_id, const void *updates,
                     int32_t index_st, int32_t num_updates) {
    int32_t partition_num = GlobalContext::GetPartitionCommChannelIndex(row_id);
    int32_t buff_pushed = oplog_partitions_[partition_num]->DenseBatchInc(
        row_id, updates, index_st, num_updates);
    return (buff_pushed == 1) ? partition_num : -1;
  }

  bool FindOpLog(int32_t row_id, OpLogAccessor *oplog_accessor) {
    int32_t partition_num = GlobalContext::GetPartitionCommChannelIndex(row_id);
    return oplog_partitions_[partition_num]->FindOpLog(row_id, oplog_accessor);
  }

  bool FindInsertOpLog(int32_t row_id, OpLogAccessor *oplog_accessor) {
    int32_t partition_num = GlobalContext::GetPartitionCommChannelIndex(row_id);
    return oplog_partitions_[partition_num]->FindInsertOpLog(
        row_id, oplog_accessor);
  }

  AbstractRowOpLog *FindOpLog(int32_t row_id) {
    int32_t partition_num = GlobalContext::GetPartitionCommChannelIndex(row_id);
    return oplog_partitions_[partition_num]->FindOpLog(row_id);
  }

  AbstractRowOpLog *FindInsertOpLog(int32_t row_id) {
    int32_t partition_num = GlobalContext::GetPartitionCommChannelIndex(row_id);
    return oplog_partitions_[partition_num]->FindInsertOpLog(row_id);
  }

  bool FindAndLock(int32_t row_id, OpLogAccessor *oplog_accessor) {
    int32_t partition_num = GlobalContext::GetPartitionCommChannelIndex(row_id);
    return oplog_partitions_[partition_num]->FindAndLock(row_id,
                                                         oplog_accessor);
  }

  bool GetEraseOpLog(int32_t row_id, AbstractRowOpLog **row_oplog_ptr) {
    int32_t partition_num = GlobalContext::GetPartitionCommChannelIndex(row_id);
    return oplog_partitions_[partition_num]->GetEraseOpLog(row_id,
                                                           row_oplog_ptr);
  }

  bool GetEraseOpLogIf(int32_t row_id,
                       GetOpLogTestFunc test,
                       void *test_args, AbstractRowOpLog **row_oplog_ptr) {
    int32_t partition_num = GlobalContext::GetPartitionCommChannelIndex(row_id);
    return oplog_partitions_[partition_num]->GetEraseOpLogIf(row_id, test,
                                                            test_args,
                                                            row_oplog_ptr);
  }

  bool GetInvalidateOpLogMeta(int32_t row_id,
                              RowOpLogMeta *row_oplog_meta) {
    int32_t partition_num = GlobalContext::GetPartitionCommChannelIndex(row_id);
    return oplog_partitions_[partition_num]->GetInvalidateOpLogMeta(
        row_id, row_oplog_meta);
  }

  AbstractAppendOnlyBuffer *GetAppendOnlyBuffer(int32_t comm_channel_idx) {
    return oplog_partitions_[comm_channel_idx]->GetAppendOnlyBuffer(comm_channel_idx);
  }

  void PutBackBuffer(int32_t comm_channel_idx, AbstractAppendOnlyBuffer* buff) {
    oplog_partitions_[comm_channel_idx]->PutBackBuffer(comm_channel_idx, buff);
  }

  void FlushOpLog() {
    for (auto oplog_partition_ptr : oplog_partitions_) {
      oplog_partition_ptr->FlushOpLog();
    }
  }

private:
  std::vector<AppendOnlyOpLogPartition*> oplog_partitions_;
};

}   // namespace petuum
