#pragma once

// author: jinliang

#include <petuum_ps_common/include/abstract_row.hpp>

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
  ClientRow(int32_t clock __attribute__((unused)), AbstractRow* row_data,
            bool use_ref_count):
      num_refs_(0),
      row_data_ptr_(row_data)
  {
    if (use_ref_count) {
      IncRef_ = &ClientRow::DoIncRef;
      DecRef_ = &ClientRow::DoDecRef;
    } else {
      IncRef_ = &ClientRow::DoNothing;
      DecRef_ = &ClientRow::DoNothing;
    }
  }

  virtual ~ClientRow() { }

  virtual void SetClock(int32_t clock __attribute__((unused))) { }

  virtual int32_t GetClock() const {
    return -1;
  }

  AbstractRow *GetRowDataPtr() {
    CHECK(this != 0);
    CHECK(row_data_ptr_.get() != 0);
    return row_data_ptr_.get();
  }

  // Whether this ClientRow has 0 reference count.
  bool HasZeroRef() const { return (num_refs_ == 0); }

  // Increment reference count (thread safe).
  void IncRef() {
    (this->*IncRef_)();
  }

  // Decrement reference count (thread safe).
  void DecRef() {
    (this->*DecRef_)();
  }

  int32_t get_num_refs() const { return num_refs_; }

private:  // private members

  void DoIncRef() { ++num_refs_; }
  void DoDecRef() { --num_refs_; }
  void DoNothing() { }

  typedef void (ClientRow::*IncDecRefFunc)();

  std::atomic<int32_t> num_refs_;

  // Row data stored in user-defined data structure ROW. We assume ROW to be
  // thread-safe. (pptr stands for pointer to pointer).
  const std::unique_ptr<AbstractRow> row_data_ptr_;

  IncDecRefFunc IncRef_;
  IncDecRefFunc DecRef_;
};

}  // namespace petuum
