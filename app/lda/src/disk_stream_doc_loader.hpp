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
#include "abstract_doc_loader.hpp"
#include "disk_stream_reader.hpp"
#include <petuum_ps_common/comm_bus/comm_bus.hpp>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <cstdint>

namespace lda {

// DiskStreamMasterDocLoader creates a background thread running
// DiskStreamReader. DiskStreamMasterDocLoader needs to outlive both
// DiskStreamPartitionDocLoader and DiskStreamReader (as it maintains CommBus
// ownership).
//
// IMPORTANT: The DiskStreamPartitionDocLoader needs to be run on different
// threads, or it'll deadlock. (Technical reasons: we use a process barrier to
// facilitate zeromq binding, which is used in CommBus. CommBus is used for
// communication between the reader thread and the worker threads.)
class DiskStreamMasterDocLoader : public AbstractMasterDocLoader {
public:
  DiskStreamMasterDocLoader(const std::string& doc_file,
      int32_t num_partitions, int32_t num_topics, int32_t batch_size);

  ~DiskStreamMasterDocLoader();

  // See AbstractMasterDocLoader.
  AbstractPartitionDocLoader* GetPartitionDocLoader(int partition_id);

private:
  std::unique_ptr<DiskStreamReader> reader_;
  std::thread reader_thread_;

  // DiskStreamReader will initiate a sweep over data set or terminate
  // based on msg received through cmd_comm_bus_ (from client workere
  // thread 0).
  std::unique_ptr<petuum::CommBus> cmd_comm_bus_;

  // DiskStreamReader sends DocBatch pointers to FastDiskPartitionDocLoader
  // (run by worker threads) through doc_comm_bus_, and receives DocBatch
  // pointers back after they are processed..
  std::unique_ptr<petuum::CommBus> doc_comm_bus_;

  // Used to ensure all threads are registered against CommBus before
  // ConnectTo.
  std::unique_ptr<boost::barrier> process_barrier_;
};

struct DiskStreamPartitionDocLoaderConfig {
  int32_t num_partitions;
  int32_t partition_id;
  int32_t num_docs;
  int32_t num_topics;
  petuum::CommBus* cmd_comm_bus;
  petuum::CommBus* doc_comm_bus;
  boost::barrier* process_barrier;
};

// Stream documents within certain global range.
class DiskStreamPartitionDocLoader : public AbstractPartitionDocLoader {
public:
  // Does not take ownership of the comm_bus's.
  DiskStreamPartitionDocLoader(
      const DiskStreamPartitionDocLoaderConfig& config);

  ~DiskStreamPartitionDocLoader();

  // See AbstractPartitionDocLoader.
  LDADoc* GetOneDoc();

  // See AbstractPartitionDocLoader.
  inline bool IsEnd() const;

  // See AbstractPartitionDocLoader.
  inline void Restart();

  // See AbstractPartitionDocLoader.
  inline int GetNumDocs() const;

  // See AbstractPartitionDocLoader.
  inline int GetNumTokens() const;

private:    // private functions
  void ReturnCurrentDocBatch();

  void FetchNextDocBatch();

private:
  int32_t num_docs_;
  int32_t num_topics_;
  int32_t doc_counter_;
  bool one_data_pass_;
  int32_t num_tokens_;
  int32_t num_partitions_;
  int32_t reader_thread_id_;

  // Does not take ownership.
  petuum::CommBus* cmd_comm_bus_;
  petuum::CommBus* doc_comm_bus_;

  // Used to ensure all threads are registered against CommBus before
  // ConnectTo. Does not take ownership.
  boost::barrier* process_barrier_;

  DocBatch* doc_batch_ptr_;
  int32_t doc_batch_iter_;
};



}  // namespace lda
