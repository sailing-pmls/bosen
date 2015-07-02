#pragma once

#include <petuum_ps_common/include/abstract_row.hpp>
#include <petuum_ps_common/include/row_access.hpp>
#include <petuum_ps_common/include/configs.hpp>
#include <petuum_ps_common/storage/abstract_process_storage.hpp>
#include <petuum_ps_common/util/vector_clock_mt.hpp>
#include <petuum_ps_common/consistency/abstract_consistency_controller.hpp>
#include <petuum_ps_common/client/abstract_client_table.hpp>
#include <petuum_ps/thread/append_only_row_oplog_buffer.hpp>

#include <petuum_ps/oplog/abstract_oplog.hpp>
#include <petuum_ps/oplog/oplog_index.hpp>
#include <petuum_ps/client/thread_table.hpp>

#include <boost/thread/tss.hpp>

namespace petuum {

class ClientTable : public AbstractClientTable {
public:
  // Instantiate AbstractRow, TableOpLog, and ProcessStorage using config.
  ClientTable(int32_t table_id, const ClientTableConfig& config);

  ~ClientTable();

  void RegisterThread();
  void DeregisterThread();

  void GetAsyncForced(int32_t row_id);
  void GetAsync(int32_t row_id);
  void WaitPendingAsyncGet();
  void ThreadGet(int32_t row_id, ThreadRowAccessor *row_accessor);
  void ThreadInc(int32_t row_id, int32_t column_id, const void *update);
  void ThreadBatchInc(int32_t row_id, const int32_t* column_ids,
                      const void* updates, int32_t num_updates);
  void ThreadDenseBatchInc(int32_t row_id, const void *updates,
                           int32_t index_st, int32_t num_updates);
  void FlushThreadCache();

  ClientRow *Get(int32_t row_id, RowAccessor *row_accessor);
  void Inc(int32_t row_id, int32_t column_id, const void *update);
  void BatchInc(int32_t row_id, const int32_t* column_ids, const void* updates,
    int32_t num_updates);
  void DenseBatchInc(int32_t row_id, const void *updates, int32_t index_st,
                     int32_t num_updates);

  void Clock();
  cuckoohash_map<int32_t, bool> *GetAndResetOpLogIndex(int32_t partition_num);
  size_t GetNumRowOpLogs(int32_t partition_num);

  AbstractProcessStorage& get_process_storage () {
    return *process_storage_;
  }

  AbstractOpLog& get_oplog () {
    return *oplog_;
  }

  const AbstractRow* get_sample_row () const {
    return sample_row_;
  }

  int32_t get_row_type () const {
    return row_type_;
  }

  int32_t get_staleness() const {
    return staleness_;
  }

  bool oplog_dense_serialized() const {
    return oplog_dense_serialized_;
  }

  OpLogType get_oplog_type() const {
    return oplog_type_;
  }

  int32_t get_bg_apply_append_oplog_freq() const {
    return bg_apply_append_oplog_freq_;
  }

  int32_t get_row_oplog_type() const {
    return row_oplog_type_;
  }

  size_t get_dense_row_oplog_capacity() const {
    return dense_row_oplog_capacity_;
  }

  AppendOnlyOpLogType get_append_only_oplog_type() const {
    return append_only_oplog_type_;
  }

  bool get_no_oplog_replay() const {
    return no_oplog_replay_;
  }

private:
  const int32_t table_id_;
  const int32_t row_type_;
  const AbstractRow* const sample_row_;
  AbstractOpLog *oplog_;
  AbstractProcessStorage *process_storage_;
  AbstractConsistencyController *consistency_controller_;

  boost::thread_specific_ptr<ThreadTable> thread_cache_;
  TableOpLogIndex oplog_index_;
  int32_t staleness_;

  bool oplog_dense_serialized_;
  ClientTableConfig client_table_config_;

  const OpLogType oplog_type_;
  const int32_t bg_apply_append_oplog_freq_;
  const int32_t row_oplog_type_;
  const size_t dense_row_oplog_capacity_;
  const AppendOnlyOpLogType append_only_oplog_type_;

  const size_t row_capacity_;

  ClientRow *CreateClientRow(int32_t clock);
  ClientRow *CreateSSPClientRow(int32_t clock);

  const bool no_oplog_replay_;
};

}  // namespace petuum
