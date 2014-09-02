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
#include <petuum_ps_sn/consistency/local_ooc_consistency_controller.hpp>
#include <petuum_ps_sn/thread/context.hpp>
#include <petuum_ps_common/util/stats.hpp>
#include <petuum_ps_common/util/class_register.hpp>
#include <glog/logging.h>
#include <algorithm>
#include <sstream>

namespace petuum {

LocalOOCConsistencyController::LocalOOCConsistencyController(
  const ClientTableConfig& config,
  int32_t table_id,
  ProcessStorage& process_storage,
  const AbstractRow* sample_row,
  boost::thread_specific_ptr<ThreadTableSN> &thread_cache) :
  LocalConsistencyController(config, table_id, process_storage,
                             sample_row, thread_cache) {

  std::string db_path;
  MakeOOCDBPath(&db_path);

  leveldb::Options options;
  options.create_if_missing = true;
  options.error_if_exists = true;
  leveldb::Status status = leveldb::DB::Open(options, db_path, &db_);
  CHECK(status.ok()) << "creating db at " << db_path << " failed";
}

LocalOOCConsistencyController::~LocalOOCConsistencyController() {
  delete db_;
}

void LocalOOCConsistencyController::MakeOOCDBPath(std::string *db_path) {

  std::stringstream ss;
  ss << GlobalContextSN::get_ooc_path_prefix()
     << "/" << "table." << table_id_;

  *db_path = ss.str();
}

void LocalOOCConsistencyController::CreateInsertRow(int32_t row_id,
                                                 RowAccessor *row_accessor) {

  std::lock_guard<std::mutex> lock(create_row_mtx_);
  bool found = process_storage_.Find(row_id, row_accessor);
  if (found)
    return;

  size_t count = on_disk_row_index_.count(row_id);

  AbstractRow *row_data
    = ClassRegistry<AbstractRow>::GetRegistry().CreateObject(row_type_);

  if (count == 1) {
    leveldb::Slice key(reinterpret_cast<const char*>(&row_id), sizeof(int32_t));
    std::string value;
    leveldb::Status s = db_->Get(leveldb::ReadOptions(), key, &value);
    CHECK(s.ok());
    bool suc = row_data->Deserialize(value.data(), value.size());
    CHECK(suc);
    on_disk_row_index_.erase(row_id);
    s = db_->Delete(leveldb::WriteOptions(), key);
    CHECK(s.ok());
  } else {
    row_data->Init(row_capacity_);
  }

  ClientRow *client_row = new ClientRow(0, row_data);
  int32_t evicted_row_id;
  ClientRow *evicted_row;
  process_storage_.Insert(row_id, client_row, row_accessor, &evicted_row_id,
                          &evicted_row);

  if (evicted_row_id == -1)
    return;

  std::shared_ptr<AbstractRow> row_data_ptr;
  evicted_row->GetRowDataPtr(&row_data_ptr);

  size_t serialized_size = row_data_ptr->SerializedSize();
  uint8_t *membuf = new uint8_t[serialized_size];

  row_data_ptr->Serialize(membuf);

  leveldb::Slice key(reinterpret_cast<const char*>(&evicted_row_id),
                     sizeof(int32_t));
  leveldb::Slice value(reinterpret_cast<const char*>(membuf),
                       serialized_size);

  leveldb::Status s = db_->Put(leveldb::WriteOptions(), key, value);
  CHECK(s.ok());
  on_disk_row_index_.insert(evicted_row_id);

  delete evicted_row;
  delete[] membuf;
}

}   // namespace petuum
