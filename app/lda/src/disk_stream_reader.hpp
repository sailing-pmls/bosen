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
// Date: 2014.06.26

#pragma once

#include "lda_doc.hpp"
#include "ldb_comparator.hpp"
#include <petuum_ps_common/comm_bus/comm_bus.hpp>
#include <leveldb/db.h>
#include <boost/thread/barrier.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace lda {

// DocBatch stores a batch read from DiskStreamReader and is the unit of
// communication between Reader and PartitionDocLoader.
class DocBatch {
public:
  DocBatch() : is_last_batch_(false) { };

  // Does not take ownership of new_doc.
  void AddDoc(int32_t doc_id, LDADoc* new_doc) {
    doc_id_.push_back(doc_id);
    doc_ptr_.push_back(new_doc);
  }

  inline int32_t GetNumDocs() const {
    return doc_ptr_.size();
  }

  inline void SetIsLastBatch(bool val) {
    is_last_batch_ = val;
  }

  inline bool IsLastBatch() const {
    return is_last_batch_;
  }

  inline int32_t GetDocID(int32_t idx) const {
    return doc_id_[idx];
  }

  inline LDADoc* GetDocPtr(int32_t idx) const {
    return doc_ptr_[idx];
  }

private:
  bool is_last_batch_;
  std::vector<int32_t> doc_id_;
  std::vector<LDADoc*> doc_ptr_;
};



struct DiskStreamReaderConfig {
  int32_t num_partitions;
  int32_t batch_size;
  int32_t doc_id_begin;
  int32_t num_docs;
  std::string doc_file;
  petuum::CommBus* cmd_comm_bus;
  petuum::CommBus* doc_comm_bus;
  boost::barrier* process_barrier;
};


// Use single thread to read batches of documents according a static
// partitioning and deliver to DiskStreamPartitionDocLoader.
class DiskStreamReader {
public:
  DiskStreamReader(const DiskStreamReaderConfig& config);

  ~DiskStreamReader();

  // Suppose there are 3 partitions (partition 0, 1, 2), we partition the
  // corpus as |0|1|2|0|1|2|... The second |0| partition cannot be read until
  // the first |0| partition is sampled, returned from worker threads, written
  // back to disk, and memory freed up. So the flow is:
  //
  // 1. Read |0|1|2|
  // 2. Write |0|1|2|
  // 3. Repeat.
  void Start();

private:    // private functions.
  // ====== Helper functions for Start() =====
  void EstablishConnections();

  // Gets whether to run next iteration via cmd_comm_bus_.
  int32_t GetContinueMsg();

  void BatchWriteAndDeleteDocs(DocBatch* doc_batch);

  void ClearOutstandingBatch(int32_t partition_id);

  void CreateDocBatch(int32_t num_docs, DocBatch* doc_batch);

private:
  int32_t num_partitions_;

  // # docs per batch to deliver to the client worker threads.
  int32_t batch_size_;

  // Global doc ID.
  int32_t doc_id_begin_;
  int32_t num_docs_;
  int32_t doc_counter_;

  leveldb::DB* db_;
  leveldb::Iterator* it_;

  // See DiskStreamMasterDocLoader::cmd_comm_bus_. Does not take ownership.
  petuum::CommBus* cmd_comm_bus_;

  // See DiskStreamMasterDocLoader::doc_comm_bus_. Does not take ownership.
  petuum::CommBus* doc_comm_bus_;

  // Used to ensure all threads are registered against CommBus before
  // ConnectTo. Does not take ownership.
  boost::barrier* process_barrier_;

  IntComparator cmp_;

  // We can read the next batch for a thread if the previous read batch is
  // written back and freed, ie., outstanding_batch_[partition_id] == false.
  std::vector<bool> outstanding_batch_;

  // Buffer
  size_t buf_size_;
  uint8_t* mem_buf_;
};

}  // namespace lda
