#pragma once

// author: jinliang

#include "petuum_ps/include/abstract_row.hpp"
#include "petuum_ps/client/client_row.hpp"

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
class SSPClientRow : public ClientRow {
public:
  // ClientRow takes ownership of row_data.
  SSPClientRow(int32_t clock, AbstractRow* row_data):
      ClientRow(clock, row_data),
      clock_(clock){ }

  void SetClock(int32_t clock) {
    std::unique_lock<std::mutex> ulock(clock_mtx_);
    clock_ = clock;
  }

  int32_t GetClock() const {
    std::unique_lock<std::mutex> ulock(clock_mtx_);
    return clock_;
  }

  // Take row_data_pptr_ from other and destroy other. Existing ROW will not
  // be accessible any more, but will stay alive until all RowAccessors
  // referencing the ROW are destroyed. Accesses to SwapAndDestroy() and
  // GetRowDataPtr() must be mutually exclusive as they the former modifies
  // row_data_pptr_.
  void SwapAndDestroy(ClientRow* other) {
    clock_ = dynamic_cast<SSPClientRow*>(other)->clock_;
    ClientRow::SwapAndDestroy(other);
  }

private:  // private members
  mutable std::mutex clock_mtx_;
  int32_t clock_;
};

}  // namespace petuum
