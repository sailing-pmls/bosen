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
// author: jinliang

#include "petuum_ps/client/row_metadata.hpp"
#include "petuum_ps/include/abstract_row.hpp"
#include "boost/noncopyable.hpp"

#pragma once

namespace petuum {

// Disallow copy to avoid shared ownership of row_data.
// Allow move sematic for it to be stored in STL containers.
class ServerRow : boost::noncopyable {
public:
  ServerRow() {}
  ServerRow(const RowMetadata& metadata, AbstractRow *row_data):
    metadata_(metadata),
    row_data_(row_data) {}
  
  ~ServerRow() {
    if(row_data_ != 0)
      delete row_data_;
  }

  ServerRow(ServerRow && other): 
    metadata_(other.metadata_),
    row_data_(other.row_data_) {
    other.row_data_ = 0;
  }

  RowMetadata &get_metadata() {
    return metadata_;
  }

  void ApplyBatchInc(const int32_t *column_ids, 
    const void *update_batch, int32_t num_updates) {
    row_data_->ApplyBatchIncUnsafe(column_ids, update_batch, num_updates);
  }

  size_t SerializedSize() {
    return row_data_->SerializedSize();
  }

  size_t Serialize(void *bytes) {
    return row_data_->Serialize(bytes);
  }

private:
  RowMetadata metadata_;
  AbstractRow *row_data_;
};
}
