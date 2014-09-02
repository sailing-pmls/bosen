// Copyright (c) 2014, Sailing Lab
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the <ORGANIZATION> nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
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
