#pragma once

#include "petuum_ps/storage/process_storage.hpp"
#include "petuum_ps/include/configs.hpp"
#include "petuum_ps/include/abstract_row.hpp"
#include "petuum_ps/oplog/oplog.hpp"
#include "petuum_ps/util/vector_clock_mt.hpp"
#include "petuum_ps/client/thread_table.hpp"
#include "petuum_ps/oplog/oplog_index.hpp"
#include <boost/utility.hpp>
#include <cstdint>
#include <vector>

namespace petuum {

// Interface for consistency controller modules. For each table we associate a
// consistency controller (e.g., SSPController) that's essentially the "brain"
// that maintains a prescribed consistency policy upon each table action. All
// functions should be fully thread-safe.
class AbstractConsistencyController : boost::noncopyable {
public:
  // Controller modules rely on TableInfo to specify policy parameters (e.g.,
  // staleness in SSP). Does not take ownership of any pointer here.
  AbstractConsistencyController(const TableInfo& info,
    int32_t table_id,
    ProcessStorage& process_storage,
    TableOpLog& oplog,
    const AbstractRow* sample_row,
    boost::thread_specific_ptr<ThreadTable> &thread_cache,
    TableOpLogIndex &oplog_index) :
    process_storage_(process_storage),
    table_id_(table_id),
    oplog_(oplog),
    sample_row_(sample_row),
    thread_cache_(thread_cache),
    oplog_index_(oplog_index) { }

  virtual ~AbstractConsistencyController() { }

  virtual void GetAsync(int32_t row_id) = 0;
  virtual void WaitPendingAsnycGet() = 0;

  // Read a row in the table and is blocked until a valid row is obtained
  // (e.g., from server). A row is valid if, for example, it is sufficiently
  // fresh in SSP. The result is returned in row_accessor.
  virtual void Get(int32_t row_id, RowAccessor* row_accessor) = 0;

  // Increment (update) an entry. Does not take ownership of input argument
  // delta, which should be of template type UPDATE in Table. This may trigger
  // synchronization (e.g., in value-bound) and is blocked until consistency
  // is ensured.
  virtual void Inc(int32_t row_id, int32_t column_id, const void* delta) = 0;

  // Increment column_ids.size() entries of a row. deltas points to an array.
  virtual void BatchInc(int32_t row_id, const int32_t* column_ids,
    const void* updates, int32_t num_updates) = 0;

  // Read a row in the table and is blocked until a valid row is obtained
  // (e.g., from server). A row is valid if, for example, it is sufficiently
  // fresh in SSP. The result is returned in row_accessor.
  virtual void ThreadGet(int32_t row_id, ThreadRowAccessor* row_accessor) = 0;

  // Increment (update) an entry. Does not take ownership of input argument
  // delta, which should be of template type UPDATE in Table. This may trigger
  // synchronization (e.g., in value-bound) and is blocked until consistency
  // is ensured.
  virtual void ThreadInc(int32_t row_id, int32_t column_id, const void* delta)
  = 0;

  // Increment column_ids.size() entries of a row. deltas points to an array.
  virtual void ThreadBatchInc(int32_t row_id, const int32_t* column_ids,
    const void* updates, int32_t num_updates) = 0;

  virtual void Clock() = 0;

protected:    // common class members for all controller modules.
  // Process cache, highly concurrent.
  ProcessStorage& process_storage_;

  int32_t table_id_;

  // Controller will only write to oplog_ but never read from it, as
  // all local updates are reflected in the row values.
  TableOpLog& oplog_;

  // We use sample_row_.AddUpdates(), SubstractUpdates() as static method.
  const AbstractRow* sample_row_;

  boost::thread_specific_ptr<ThreadTable> &thread_cache_;
  TableOpLogIndex &oplog_index_;
};

}    // namespace petuum
