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

#include <petuum_ps_sn/consistency/local_consistency_controller.hpp>

#include <utility>
#include <vector>
#include <cstdint>
#include <atomic>
#include <string>
#include <leveldb/db.h>
#include <set>
#include <mutex>

namespace petuum {

class LocalOOCConsistencyController : public LocalConsistencyController {
public:
  LocalOOCConsistencyController(const ClientTableConfig& config,
    int32_t table_id,
    ProcessStorage& process_storage,
    const AbstractRow* sample_row,
    boost::thread_specific_ptr<ThreadTableSN> &thread_cache);

  ~LocalOOCConsistencyController();

protected:
  void MakeOOCDBPath(std::string *db_path);
  virtual void CreateInsertRow(int32_t row_id, RowAccessor *row_accessor);

  std::mutex create_row_mtx_;
  leveldb::DB* db_;
  std::set<int32_t> on_disk_row_index_;
};

}  // namespace petuum
