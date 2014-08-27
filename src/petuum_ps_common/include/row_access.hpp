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

#include <boost/utility.hpp>
#include <vector>
#include <utility>

#include <petuum_ps_common/include/abstract_row.hpp>
#include <petuum_ps_common/client/client_row.hpp>

namespace petuum {

class ProcessStorage;

// RowAccessor is a "smart pointer" for ROW: row_accessor.Get() gives a const
// reference.  We disallow copying of RowAccessor to avoid unnecessary
// manipulation of reference count.
class RowAccessor : boost::noncopyable {
public:
  RowAccessor() : client_row_ptr_(0), row_data_ptr_(0) { }
  ~RowAccessor() {
    Clear();
  }

  // The returned reference is guaranteed to be valid only during the
  // lifetime of this RowAccessor.
  template<typename ROW>
  inline const ROW& Get() {
    return *(dynamic_cast<ROW*>(row_data_ptr_.get()));
  }

private:
  friend class ProcessStorage;
  friend class BgWorkers;
  friend class SSPConsistencyController;
  friend class SSPPushConsistencyController;
  friend class LocalConsistencyController;
  friend class LocalOOCConsistencyController;
  friend class ThreadTable;
  friend class ThreadTableSN;

  void Clear() {
    if (client_row_ptr_ != 0) {
      client_row_ptr_->DecRef();
      client_row_ptr_ = 0;
      row_data_ptr_.reset();
    }
  }

  // Does not take ownership of client_row_ptr. Note that accesses to
  // ClientRow::SwapAndDestroy() and ClientRow::GetRowDataPtr() must be
  // mutually exclusive as the former modifies ClientRow::row_data_pptr_.
  inline void SetClientRow(ClientRow* client_row_ptr) {
    Clear();
    client_row_ptr_ = client_row_ptr;
    client_row_ptr_->IncRef();
    client_row_ptr_->GetRowDataPtr(&row_data_ptr_);
  }

  // Return client row which will stay alive throughout the lifetime of
  // RowAccessor.
  inline const ClientRow* GetClientRow() {
    return client_row_ptr_;
  }

  AbstractRow *GetRowData() {
    return row_data_ptr_.get();
  }

  ClientRow* client_row_ptr_;

  std::shared_ptr<AbstractRow> row_data_ptr_;
};

class ThreadRowAccessor : boost::noncopyable {
public:
  ThreadRowAccessor() : row_data_ptr_(0) { }
  ~ThreadRowAccessor() { }

  // The returned reference is guaranteed to be valid only during the
  // lifetime of this RowAccessor.
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
