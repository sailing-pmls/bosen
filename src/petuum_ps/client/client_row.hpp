#pragma once

// author: jinliang

#include "petuum_ps/include/abstract_row.hpp"

#include <cstdint>
#include <atomic>
#include <memory>
#include <boost/utility.hpp>
#include <mutex>
#include <glog/logging.h>

namespace petuum {

// ClientRow is a wrapper on user-defined ROW data structure (e.g., vector,
// map) with additional features:
//
// 1. Reference Counting: number of references used by application. Note the
// copy in storage itself does not contribute to the count
// 2. Row Metadata
//
// ClientRow does not provide thread-safety in itself. The locks are
// maintained in the storage and in (user-defined) ROW.
class ClientRow : boost::noncopyable {
public:
  // ClientRow takes ownership of row_data.
  ClientRow(int32_t clock __attribute__((unused)), AbstractRow* row_data):
      num_refs_(0),
      row_data_pptr_(new std::shared_ptr<AbstractRow>) {
    (*row_data_pptr_).reset(row_data);
  }

  virtual ~ClientRow() {};

  virtual void SetClock(int32_t clock __attribute__((unused))) { }

  virtual int32_t GetClock() const {
    return -1;
  }

  // Take row_data_pptr_ from other and destroy other. Existing ROW will not
  // be accessible any more, but will stay alive until all RowAccessors
  // referencing the ROW are destroyed. Accesses to SwapAndDestroy() and
  // GetRowDataPtr() must be mutually exclusive as they the former modifies
  // row_data_pptr_.
  virtual void SwapAndDestroy(ClientRow* other) {
    row_data_pptr_.swap(other->row_data_pptr_);
    delete other;
  }

  // GetRowDataPtr is not thread-safe, and must be protected by the
  // StripedLock in ProcessStorage.
  inline void GetRowDataPtr(std::shared_ptr<AbstractRow>* row_data_pptr) {
    *row_data_pptr = *row_data_pptr_;
  }

  // Whether this ClientRow has 0 reference count.
  inline bool HasZeroRef() const { return (num_refs_ == 0); }

  // Increment reference count (thread safe).
  inline void IncRef() { ++num_refs_; }

  // Decrement reference count (thread safe).
  inline void DecRef() { --num_refs_; }

private:  // private members
  std::atomic<int32_t> num_refs_;

  // Row data stored in user-defined data structure ROW. We assume ROW to be
  // thread-safe. (pptr stands for pointer to pointer).
  //
  // When ProcessStorage updates row_data_, it creates another
  // shared_ptr<ROW> and store its address in row_data_pptr_.
  std::unique_ptr<std::shared_ptr<AbstractRow> > row_data_pptr_;
};

}  // namespace petuum
