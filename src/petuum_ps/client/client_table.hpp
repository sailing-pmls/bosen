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

#include "petuum_ps/include/abstract_row.hpp"
#include "petuum_ps/include/row_access.hpp"
#include "petuum_ps/include/configs.hpp"
#include "petuum_ps/storage/process_storage.hpp"
#include "petuum_ps/oplog/oplog.hpp"
#include "petuum_ps/consistency/abstract_consistency_controller.hpp"
#include "petuum_ps/util/vector_clock_mt.hpp"

namespace petuum {

class ClientTable : boost::noncopyable {
public:
  // Instantiate AbstractRow, TableOpLog, and ProcessStorage using config.
  ClientTable(int32_t table_id, const ClientTableConfig& config);

  ~ClientTable();

  void Get(int32_t row_id, RowAccessor *row_accessor);
  void Inc(int32_t row_id, int32_t column_id, const void *update);
  void BatchInc(int32_t row_id, const int32_t* column_ids, const void* updates, 
    int32_t num_updates);

  ProcessStorage& get_process_storage () {
    return process_storage_;
  }

  TableOpLog& get_oplog () {
    return oplog_;
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
  TableOpLog oplog_;
  ProcessStorage process_storage_;
  AbstractConsistencyController *consistency_controller_;
};

}  // namespace petuum
