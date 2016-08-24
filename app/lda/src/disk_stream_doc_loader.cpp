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

#include "disk_stream_doc_loader.hpp"
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

// ======================== DiskStreamMasterDocLoader ====================

DiskStreamMasterDocLoader::DiskStreamMasterDocLoader(const std::string& doc_file,
    int32_t num_partitions, int32_t num_topics, int32_t batch_size) :
  AbstractMasterDocLoader(doc_file, num_partitions, num_topics),
  // 0: reader thread. 1: client worker thread 0.
  cmd_comm_bus_(new petuum::CommBus(0, 1, 1)),
  // Thread num_partitions is the reader thread, and 0 ~ num_partitions-1 are
  // the client worker threads.
  doc_comm_bus_(new petuum::CommBus(0, num_partitions, num_partitions)),
  process_barrier_(new boost::barrier(num_partitions + 1)) {
    DiskStreamReaderConfig reader_config;
    reader_config.num_partitions = num_partitions_;
    reader_config.batch_size = batch_size;
    reader_config.doc_id_begin = doc_id_begin_;
    reader_config.num_docs = doc_id_end_ - doc_id_begin_;
    reader_config.doc_file = doc_file;
    reader_config.cmd_comm_bus = cmd_comm_bus_.get();
    reader_config.doc_comm_bus = doc_comm_bus_.get();
    reader_config.process_barrier = process_barrier_.get();
    reader_.reset(new DiskStreamReader(reader_config));
    reader_thread_ = std::thread(&lda::DiskStreamReader::Start,
        std::ref(*reader_));
  }

DiskStreamMasterDocLoader::~DiskStreamMasterDocLoader() {
  reader_thread_.join();
}

AbstractPartitionDocLoader*
DiskStreamMasterDocLoader::GetPartitionDocLoader(int partition_id) {
  DiskStreamPartitionDocLoaderConfig config;
  config.num_partitions = num_partitions_;
  config.partition_id = partition_id;
  config.num_docs = partition_doc_id_end_[partition_id] -
    partition_doc_id_begin_[partition_id];
  config.num_topics = num_topics_;
  config.cmd_comm_bus = cmd_comm_bus_.get();
  config.doc_comm_bus = doc_comm_bus_.get();
  config.process_barrier = process_barrier_.get();
  return new DiskStreamPartitionDocLoader(config);
}

// ======================== DiskStreamPartitionDocLoader ====================

DiskStreamPartitionDocLoader::DiskStreamPartitionDocLoader(
    const DiskStreamPartitionDocLoaderConfig& config)
: AbstractPartitionDocLoader(config.partition_id),
  num_docs_(config.num_docs), num_topics_(config.num_topics),
  doc_counter_(0), one_data_pass_(false), num_tokens_(0),
  num_partitions_(config.num_partitions),
  reader_thread_id_(num_partitions_), doc_batch_ptr_(0),
  doc_batch_iter_(0) {
    CHECK_NOTNULL(config.cmd_comm_bus);
    CHECK_NOTNULL(config.doc_comm_bus);
    CHECK_NOTNULL(config.process_barrier);
    cmd_comm_bus_ = config.cmd_comm_bus;
    doc_comm_bus_ = config.doc_comm_bus;
    process_barrier_ = config.process_barrier;
    int32_t my_id = 0;
    if (partition_id_ == 0) {
      // Only partition 0 needs to give continuation command to
      // DiskStreamReader.
      my_id = 1;
      petuum::CommBus::Config cmd_comm_bus_config(my_id,
          petuum::CommBus::kNone, "");
      cmd_comm_bus_->ThreadRegister(cmd_comm_bus_config);
      process_barrier_->wait();
      cmd_comm_bus_->ConnectTo(0, reinterpret_cast<void*>(&my_id),
          sizeof(int32_t));
    } else {
      process_barrier_->wait();
    }

    my_id = partition_id_;
    petuum::CommBus::Config doc_comm_bus_config(my_id, petuum::CommBus::kNone,
        "");
    doc_comm_bus_->ThreadRegister(doc_comm_bus_config);
    process_barrier_->wait();
    int32_t reader_id = num_partitions_;
    doc_comm_bus_->ConnectTo(reader_id, reinterpret_cast<void*>(&my_id),
        sizeof(int32_t));
    process_barrier_->wait();
    Restart();
  }

DiskStreamPartitionDocLoader::~DiskStreamPartitionDocLoader() {
  if (partition_id_ == 0) {
    // Send shutdown signal to the reader thread.
    int32_t cont = -1;
    cmd_comm_bus_->SendInProc(0, &cont, sizeof(cont));
    cmd_comm_bus_->ThreadDeregister();
  }
  ReturnCurrentDocBatch();
  doc_comm_bus_->ThreadDeregister();
}

LDADoc* DiskStreamPartitionDocLoader::GetOneDoc() {
  CHECK_NOTNULL(doc_batch_ptr_);
  CHECK(!IsEnd());
  if (doc_batch_iter_ == doc_batch_ptr_->GetNumDocs()) {
    ReturnCurrentDocBatch();
    FetchNextDocBatch();
  }
  LDADoc* doc = doc_batch_ptr_->GetDocPtr(doc_batch_iter_);
  if (doc->GetNumTopics() != num_topics_) {
    doc->RandomInitTopics(num_topics_);
  }
  if (!one_data_pass_) {
    num_tokens_ += doc->GetNumTokens();
  }
  ++doc_batch_iter_;
  return doc;
}

bool DiskStreamPartitionDocLoader::IsEnd() const {
  CHECK_NOTNULL(doc_batch_ptr_);
  return doc_batch_ptr_->IsLastBatch()
    && doc_batch_iter_ == doc_batch_ptr_->GetNumDocs();
}

void DiskStreamPartitionDocLoader::Restart() {
  if (doc_batch_ptr_ != 0) {
    // Restart() is called not by the constructor.
    CHECK(IsEnd())
      << "Cannot restart a PartitionDocLoader halfway in the iteration";
    if (!one_data_pass_) {
      one_data_pass_ = true;
    }
    ReturnCurrentDocBatch();
  }
  if (partition_id_ == 0) {
    // Send continuation signal to the reader:
    //  0: first iteration.
    //  1: second+ iteration.
    //  -1: terminate.
    int32_t cont = (doc_batch_ptr_ == 0) ? 0 : 1;
    cmd_comm_bus_->SendInProc(0, &cont, sizeof(cont));
  }
  FetchNextDocBatch();
}

int DiskStreamPartitionDocLoader::GetNumDocs() const {
  return num_docs_;
}

int DiskStreamPartitionDocLoader::GetNumTokens() const {
  if (!one_data_pass_) {
    LOG(WARNING) << "Getting incomplete count of num_tokens.";
  }
  return num_tokens_;
}

void DiskStreamPartitionDocLoader::ReturnCurrentDocBatch() {
  doc_comm_bus_->SendInProc(reader_thread_id_, &doc_batch_ptr_,
      sizeof(DocBatch*));
}

void DiskStreamPartitionDocLoader::FetchNextDocBatch() {
  int32_t sender_id;
  zmq::message_t zmq_msg;
  doc_comm_bus_->RecvInProc(&sender_id, &zmq_msg);
  CHECK_EQ(reader_thread_id_, sender_id);
  doc_batch_ptr_ = *reinterpret_cast<DocBatch**>(zmq_msg.data());
  doc_batch_iter_ = 0;
}

}  // namespace lda
