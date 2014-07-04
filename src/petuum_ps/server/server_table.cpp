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
#include <petuum_ps/server/server_table.hpp>

#include <sstream>
#include <leveldb/db.h>

namespace petuum {

void ServerTable::MakeSnapShotFileName(const std::string &snapshot_dir,
                                       int32_t server_id, int32_t table_id, int32_t clock,
                                       std::string *filename) const {

  std::stringstream ss;
  ss << snapshot_dir << "/server_table" << ".server-" << server_id
     << ".table-" << table_id << ".clock-" << clock
     << ".db";
  *filename = ss.str();
}

void ServerTable::TakeSnapShot(const std::string &snapshot_dir,
                               int32_t server_id, int32_t table_id, int32_t clock) const {

  std::string db_name;
  MakeSnapShotFileName(snapshot_dir, server_id, table_id, clock, &db_name);

  leveldb::DB* db;
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::Status status = leveldb::DB::Open(options, db_name, &db);

  uint8_t *mem_buff = new uint8_t[512];
  int32_t buff_size = 512;
  for (auto row_iter = storage_.begin(); row_iter != storage_.end();
       row_iter++) {
    int32_t serialized_size = row_iter->second.SerializedSize();
    if (buff_size < serialized_size) {
      delete[] mem_buff;
      buff_size = serialized_size;
      mem_buff = new uint8_t[buff_size];
    }
    row_iter->second.Serialize(mem_buff);
    leveldb::Slice key(reinterpret_cast<const char*>(&(row_iter->first)),
                       sizeof(int32_t));
    leveldb::Slice value(reinterpret_cast<const char*>(mem_buff),
                         serialized_size);
    db->Put(leveldb::WriteOptions(), key, value);
  }
  delete[] mem_buff;
  delete db;
}

void ServerTable::ReadSnapShot(const std::string &resume_dir,
                               int32_t server_id, int32_t table_id, int32_t clock) {

  std::string db_name;
  MakeSnapShotFileName(resume_dir, server_id, table_id, clock, &db_name);

  leveldb::DB* db;
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::Status status = leveldb::DB::Open(options, db_name, &db);

  leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    int32_t row_id = *(reinterpret_cast<const int32_t*>(it->key().data()));
        const uint8_t *row_data
            = reinterpret_cast<const uint8_t*>(it->value().data());
        size_t row_data_size = it->value().size();

        int32_t row_type = table_info_.row_type;
            AbstractRow *abstract_row
                = ClassRegistry<AbstractRow>::GetRegistry().CreateObject(row_type);
            abstract_row->Deserialize(row_data, row_data_size);

            storage_.insert(std::make_pair(row_id, ServerRow(abstract_row)));
            VLOG(0) << "ReadSnapShot, row_id = " << row_id;
  }
  delete it;
  delete db;
}
}
