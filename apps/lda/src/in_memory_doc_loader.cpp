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
// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.06.19

#include "in_memory_doc_loader.hpp"
#include <petuum_ps_common/include/petuum_ps.hpp>
#include <leveldb/db.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <glog/logging.h>

namespace lda {

// ============ InMemoryMasterDocLoader ==============

InMemoryMasterDocLoader::InMemoryMasterDocLoader(
    const std::string& doc_file, int32_t num_partitions, int32_t num_topics) :
  AbstractMasterDocLoader(doc_file, num_partitions, num_topics),
  doc_partitions_ptr_(num_partitions) {
    petuum::HighResolutionTimer read_timer;
    leveldb::Options options;
    options.comparator = &cmp_;
    leveldb::DB *db;
    leveldb::Status status = leveldb::DB::Open(options, doc_file, &db);
    CHECK(status.ok()) << "Open level db failed, path: " << doc_file
      << " Error: " << status.ToString();

    leveldb::ReadOptions read_options;
    read_options.fill_cache = false;

    leveldb::Iterator* it = db->NewIterator(read_options);
    int doc_id = doc_id_begin_;
    int curr_partition_id = 0;
    for (int i = 0; i < num_partitions_; ++i) {
      doc_partitions_ptr_[i] = new std::vector<LDADoc>();
    }
    std::vector<LDADoc>* doc_partition_ptr =
      doc_partitions_ptr_[curr_partition_id];
    // Read everything in the db.
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      if (doc_id == partition_doc_id_end_[curr_partition_id]) {
        ++curr_partition_id;
        doc_partition_ptr = doc_partitions_ptr_[curr_partition_id];
      }
      const uint8_t *doc_data
        = reinterpret_cast<const uint8_t*>(it->value().data());
      doc_partition_ptr->emplace_back();
      LDADoc& new_doc = (*doc_partition_ptr)[doc_partition_ptr->size() - 1];
      new_doc.Deserialize(doc_data);
      // Check if the topics are initialized.
      if (new_doc.GetNumTopics() != num_topics_) {
        new_doc.RandomInitTopics(num_topics_);
      }
      ++doc_id;
    }
    CHECK(it->status().ok());  // Check for any errors found during the scan
    delete it;
    delete db;
    int num_docs_read = doc_id - doc_id_begin_;
    LOG(INFO) << "InMemoryMasterDocLoader reads (and initializes) "
      << num_docs_read << " documents in " << read_timer.elapsed()
      << " seconds.";
    CHECK_EQ(num_docs_, num_docs_read)
      << "num_docs in meta-file disagrees with the actual leveldb reading.";
  }

// This method is thread-safe.
AbstractPartitionDocLoader* InMemoryMasterDocLoader::GetPartitionDocLoader(
    int partition_id) {
  CHECK_NOTNULL(doc_partitions_ptr_[partition_id]);
  std::vector<LDADoc>* doc_partition_ptr = doc_partitions_ptr_[partition_id];
  doc_partitions_ptr_[partition_id] = 0;    // disown the docs.
  return new InMemoryPartitionDocLoader(partition_id, doc_partition_ptr);
}

InMemoryMasterDocLoader::~InMemoryMasterDocLoader() {
  for (int i = 0; i < num_partitions_; ++i) {
    if (doc_partitions_ptr_[i] != 0) {
      delete doc_partitions_ptr_[i];
    }
  }
}

// ============ InMemoryPartitionDocLoader ==============

InMemoryPartitionDocLoader::InMemoryPartitionDocLoader(
    int32_t partition_id,
    std::vector<LDADoc>* partition_docs)
  : AbstractPartitionDocLoader(partition_id),
  partition_docs_(partition_docs), curr_(partition_docs_->begin()),
  num_tokens_(0) {
    for (auto& doc : *partition_docs_) {
      num_tokens_ += doc.GetNumTokens();
    }
  }


LDADoc* InMemoryPartitionDocLoader::GetOneDoc() {
  CHECK(curr_ != partition_docs_->end());
  auto& this_doc = *curr_;
  ++curr_;
  return &this_doc;
}

bool InMemoryPartitionDocLoader::IsEnd() const {
  return curr_ == partition_docs_->end();
}

void InMemoryPartitionDocLoader::Restart() {
  curr_ = partition_docs_->begin();
}

int InMemoryPartitionDocLoader::GetNumDocs() const {
  return partition_docs_->size();
}

int InMemoryPartitionDocLoader::GetNumTokens() const {
  return num_tokens_;
}

}  // namespace lda
