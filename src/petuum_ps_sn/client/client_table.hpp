
#pragma once

#include <petuum_ps_common/include/abstract_row.hpp>
#include <petuum_ps_common/include/row_access.hpp>
#include <petuum_ps_common/include/configs.hpp>
#include <petuum_ps_common/storage/process_storage.hpp>
#include <petuum_ps_common/consistency/abstract_consistency_controller.hpp>
#include <petuum_ps_common/util/vector_clock_mt.hpp>
#include <petuum_ps_common/client/abstract_client_table.hpp>
#include <petuum_ps_sn/client/thread_table.hpp>

#include <boost/thread/tss.hpp>

namespace petuum {

class ClientTableSN : public AbstractClientTable {
public:
  // Instantiate AbstractRow, TableOpLog, and ProcessStorage using config.
  ClientTableSN(int32_t table_id, const ClientTableConfig& config);

  ~ClientTableSN();

  void RegisterThread();

  void GetAsync(int32_t row_id);
  void WaitPendingAsyncGet();
  void ThreadGet(int32_t row_id, ThreadRowAccessor *row_accessor);
  void ThreadInc(int32_t row_id, int32_t column_id, const void *update);
  void ThreadBatchInc(int32_t row_id, const int32_t* column_ids,
                      const void* updates,
                      int32_t num_updates);
  void FlushThreadCache();

  void Get(int32_t row_id, RowAccessor *row_accessor);
  void Inc(int32_t row_id, int32_t column_id, const void *update);
  void BatchInc(int32_t row_id, const int32_t* column_ids, const void* updates,
    int32_t num_updates);

  void Clock();

  ProcessStorage& get_process_storage () {
    return process_storage_;
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
  ProcessStorage process_storage_;
  AbstractConsistencyController *consistency_controller_;

  boost::thread_specific_ptr<ThreadTableSN> thread_cache_;
};

}  // namespace petuum
