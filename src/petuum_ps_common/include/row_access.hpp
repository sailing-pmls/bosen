#pragma once

#include <boost/utility.hpp>
#include <vector>
#include <utility>

#include <petuum_ps_common/include/abstract_row.hpp>
#include <petuum_ps_common/client/client_row.hpp>

namespace petuum {

class ProcessStorage;

/**
 * RowAccessor is a "smart pointer" for ROW: row_accessor.Get() gives a const
 * reference.  We disallow copying of RowAccessor to avoid unnecessary
 * manipulation of reference count.
 */
class RowAccessor : boost::noncopyable {
public:
  RowAccessor() : client_row_ptr_(0) { }
  ~RowAccessor() {
    Clear();
  }

  /**
   * The returned reference is guaranteed to be valid only during the
   * lifetime of this RowAccessor.
   */
  template<typename ROW>
  inline const ROW& Get() {
    return *(dynamic_cast<ROW*>(client_row_ptr_->GetRowDataPtr()));
  }

private:
  friend class BoundedDenseProcessStorage;
  friend class BoundedSparseProcessStorage;
  friend class AbstractBgWorker;
  friend class SSPBgWorker;
  friend class SSPPushBgWorker;
  friend class SSPAggrBgWorker;
  friend class SSPConsistencyController;
  friend class SSPPushConsistencyController;
  friend class SSPAggrConsistencyController;
  friend class SSPAggrValueConsistencyController;
  friend class LocalConsistencyController;
  friend class LocalOOCConsistencyController;
  friend class ThreadTable;
  friend class ThreadTableSN;

  void Clear() {
    if (client_row_ptr_ != 0) {
      client_row_ptr_->DecRef();
      client_row_ptr_ = 0;
    }
  }

  /**
   * Does not take ownership of client_row_ptr.
   */
  inline void SetClientRow(ClientRow* client_row_ptr) {
    //Clear();
    client_row_ptr_ = client_row_ptr;
    client_row_ptr_->IncRef();
  }

  /**
   * Returned client row which will stay alive throughout the lifetime of
   * RowAccessor.
   */
  inline const ClientRow* GetClientRow() {
    return client_row_ptr_;
  }

  AbstractRow *GetRowData() {
    return client_row_ptr_->GetRowDataPtr();
  }

  ClientRow* client_row_ptr_;
};

class ThreadRowAccessor : boost::noncopyable {
public:
  ThreadRowAccessor() : row_data_ptr_(0) { }
  ~ThreadRowAccessor() { }

  /**
   * The returned reference is guaranteed to be valid only during the
   * lifetime of this RowAccessor.
   */
  template<typename ROW>
  inline const ROW& Get() {
    return *(dynamic_cast<ROW*>(row_data_ptr_));
  }

private:
  friend class SSPConsistencyController;
  friend class SSPPushConsistencyController;
  friend class LocalConsistencyController;
  friend class LocalOOCConsistencyController;
  friend class ThreadTable;
  friend class ThreadTableSN;

  AbstractRow *row_data_ptr_;
};

}  // namespace petuum
