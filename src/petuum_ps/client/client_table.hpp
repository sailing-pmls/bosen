
#pragma once

#include "petuum_ps/include/abstract_row.hpp"
#include "petuum_ps/include/row_access.hpp"
#include "petuum_ps/include/configs.hpp"
#include "petuum_ps/storage/process_storage.hpp"
#include "petuum_ps/oplog/oplog.hpp"
#include "petuum_ps/consistency/abstract_consistency_controller.hpp"
#include "petuum_ps/util/vector_clock_mt.hpp"
#include "petuum_ps/client/thread_table.hpp"
#include "petuum_ps/oplog/oplog_index.hpp"

#include <boost/thread/tss.hpp>

namespace petuum {

class ClientTable : boost::noncopyable {
public:
  // Instantiate AbstractRow, TableOpLog, and ProcessStorage using config.
  ClientTable(int32_t table_id, const ClientTableConfig& config);

  ~ClientTable();

  void RegisterThread();

  void GetAsync(int32_t row_id);
  void WaitPendingAsyncGet();
  void ThreadGet(int32_t row_id, ThreadRowAccessor *row_accessor);
  void ThreadInc(int32_t row_id, int32_t column_id, const void *update);
  void ThreadBatchInc(int32_t row_id, const int32_t* column_ids,
                      const void* updates,
                      int32_t num_updates);

  void Get(int32_t row_id, RowAccessor *row_accessor);
  void Inc(int32_t row_id, int32_t column_id, const void *update);
  void BatchInc(int32_t row_id, const int32_t* column_ids, const void* updates,
    int32_t num_updates);

  void Clock();
  cuckoohash_map<int32_t, bool> *GetAndResetOpLogIndex(int32_t client_table);

  ProcessStorage& get_process_storage () {
    return process_storage_;
  }

  TableOpLog& get_oplog () {
    return oplog_;
  }

  const AbstractRow* get_sample_row () const {
    return sample_row_;
  }

  int32_t get_row_type () const {
    return row_type_;
  }

private:
  int32_t table_id_;
  int32_t row_type_;
  const AbstractRow* const sample_row_;
  TableOpLog oplog_;
  ProcessStorage process_storage_;
  AbstractConsistencyController *consistency_controller_;

  boost::thread_specific_ptr<ThreadTable> thread_cache_;
  TableOpLogIndex oplog_index_;
};

}  // namespace petuum
