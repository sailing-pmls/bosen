
#pragma once

#include <petuum_ps_common/include/abstract_row.hpp>
#include <petuum_ps_common/include/row_access.hpp>
#include <petuum_ps_common/include/configs.hpp>
#include <petuum_ps_common/client/client_row.hpp>

#include <libcuckoo/cuckoohash_map.hh>

namespace petuum {

class AbstractClientTable : boost::noncopyable {
public:
  AbstractClientTable() { }

  virtual ~AbstractClientTable() { }

  virtual void RegisterThread() = 0;

  virtual void GetAsyncForced(int32_t row_id) = 0;
  virtual void GetAsync(int32_t row_id) = 0;
  virtual void WaitPendingAsyncGet() = 0;
  virtual void ThreadGet(int32_t row_id, ThreadRowAccessor *row_accessor) = 0;
  virtual void ThreadInc(int32_t row_id, int32_t column_id,
                         const void *update) = 0;
  virtual void ThreadBatchInc(int32_t row_id, const int32_t* column_ids,
                              const void* updates,
                              int32_t num_updates) = 0;
  virtual void ThreadDenseBatchInc(int32_t row_id, const void *updates,
                                  int32_t index_st,
                                  int32_t num_updates) = 0;
  virtual void FlushThreadCache() = 0;

  virtual ClientRow *Get(int32_t row_id, RowAccessor *row_accessor,
                         int32_t clock) = 0;
  virtual void Inc(int32_t row_id, int32_t column_id, const void *update) = 0;
  virtual void BatchInc(int32_t row_id, const int32_t* column_ids,
                        const void* updates,
                        int32_t num_updates) = 0;
  virtual void DenseBatchInc(int32_t row_id, const void *updates,
                             int32_t index_st,
                             int32_t num_updates) = 0;
  virtual void Clock() = 0;

  virtual int32_t get_row_type() const = 0;
};

}  // namespace petuum
