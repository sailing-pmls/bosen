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

#include "lda_doc.hpp"
#include "disk_stream_reader.hpp"
#include "ldb_comparator.hpp"
#include <petuum_ps_common/comm_bus/comm_bus.hpp>
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <boost/thread/barrier.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace lda {

DiskStreamReader::DiskStreamReader(const DiskStreamReaderConfig& config) :
  num_partitions_(config.num_partitions),
  batch_size_(config.batch_size),
  doc_id_begin_(config.doc_id_begin),
  num_docs_(config.num_docs),
  cmd_comm_bus_(config.cmd_comm_bus),
  doc_comm_bus_(config.doc_comm_bus),
  process_barrier_(config.process_barrier),
  outstanding_batch_(num_partitions_),
  buf_size_(1024), mem_buf_(new uint8_t[buf_size_]) {
    leveldb::Options options;
    options.comparator = &cmp_;
    leveldb::Status status = leveldb::DB::Open(options, config.doc_file, &db_);
    CHECK(status.ok()) << "Open level db failed, path: " << config.doc_file
      << " Error: " << status.ToString();
  }

DiskStreamReader::~DiskStreamReader() {
  delete db_;
  delete[] mem_buf_;
}

void DiskStreamReader::Start() {
  EstablishConnections();

  leveldb::ReadOptions read_options;
  read_options.fill_cache = false;
  int32_t num_batches_per_partition = std::ceil(static_cast<float>(num_docs_)
      / (batch_size_ * num_partitions_));

  // Last batch could be smaller than batch_size_.
  int32_t num_remained_docs = num_docs_ % (batch_size_ * num_partitions_);
  int32_t last_batch_size = num_remained_docs / num_partitions_;

  // The last partition in the last batch could be different from
  // last_batch_size.
  int32_t last_partition_last_batch_size = num_remained_docs
    - last_batch_size * (num_partitions_ - 1);

  int32_t cont = GetContinueMsg();
  CHECK_EQ(0, cont) << "cont has to be 0 for the first iteration.";
  std::fill(outstanding_batch_.begin(), outstanding_batch_.end(), false);
  while (cont >= 0) {
    // Flush out all pending writes before creating iterator.
    for (int partition_id = 0; partition_id < num_partitions_;
        ++partition_id) {
      ClearOutstandingBatch(partition_id);
    }
    it_ = db_->NewIterator(read_options);
    it_->SeekToFirst();
    doc_counter_ = 0;
    for (int i = 0; i < num_batches_per_partition; ++i) {
      for (int partition_id = 0; partition_id < num_partitions_;
          ++partition_id) {
        // Return only after previous batch for partition partition_id is written
        // back to disk and freed from memory.
        ClearOutstandingBatch(partition_id);

        // Determine batch_size.
        int32_t this_batch_size = 0;
        if (i < num_batches_per_partition - 1) {
          // Not the last batch, thus a complete batch.
          this_batch_size = batch_size_;
        } else if (partition_id < num_partitions_ - 1) {
          // last batch, but not the last partition.
          this_batch_size = last_batch_size;
        } else {
          // last batch and last partition.
          this_batch_size = last_partition_last_batch_size;
        }
        DocBatch* doc_batch = new DocBatch();
        CreateDocBatch(this_batch_size, doc_batch);

        if (i == num_batches_per_partition -  1) {
          doc_batch->SetIsLastBatch(true);
        }

        // Send just the pointer over as it's shared memory.
        doc_comm_bus_->SendInProc(partition_id,
            &doc_batch, sizeof(doc_batch));
        outstanding_batch_[partition_id] = true;
      }
    }
    cont = GetContinueMsg();
    delete it_;
  }
  // Write the last batch back before exiting.
  for (int partition_id = 0; partition_id < num_partitions_;
      ++partition_id) {
    ClearOutstandingBatch(partition_id);
  }
  cmd_comm_bus_->ThreadDeregister();
  doc_comm_bus_->ThreadDeregister();
}

void DiskStreamReader::EstablishConnections() {
  // DiskStreamReader waits for DiskStreamPartitionDocLoader to
  // initiate connection.
  int32_t my_id = 0;
  petuum::CommBus::Config cmd_comm_bus_config(my_id,
      petuum::CommBus::kInProc, "");
  cmd_comm_bus_->ThreadRegister(cmd_comm_bus_config);
  process_barrier_->wait();
  int sender_id;
  // initial message is ignored.
  zmq::message_t zmq_msg;
  cmd_comm_bus_->RecvInProc(&sender_id, &zmq_msg);
  CHECK_EQ(1, sender_id);

  my_id = num_partitions_;
  petuum::CommBus::Config doc_comm_bus_config(my_id,
      petuum::CommBus::kInProc, "");
  doc_comm_bus_->ThreadRegister(doc_comm_bus_config);
  process_barrier_->wait();
  std::vector<bool> partition_conn(num_partitions_);
  std::fill(partition_conn.begin(), partition_conn.end(), false);
  for (int i = 0; i < num_partitions_; ++i) {
    // initial message should be empty.
    doc_comm_bus_->RecvInProc(&sender_id, &zmq_msg);
    CHECK(!partition_conn[sender_id])
      << "Repeated connection from partition " << sender_id;
    partition_conn[sender_id] = true;
  }
  for (int i = 0; i < num_partitions_; ++i) {
    CHECK(partition_conn[i]) << "Partition " << i << " is not connected.";
  }
  process_barrier_->wait();
  LOG(INFO) << "DiskStreamReader establishes connections.";
}

int32_t DiskStreamReader::GetContinueMsg() {
  int32_t sender_id;
  zmq::message_t zmq_msg;
  cmd_comm_bus_->RecvInProc(&sender_id, &zmq_msg);
  CHECK_EQ(1, sender_id);
  return *reinterpret_cast<int32_t*>(zmq_msg.data());
}

void DiskStreamReader::BatchWriteAndDeleteDocs(DocBatch* doc_batch) {
  // Batch write.
  leveldb::WriteBatch batch;
  int total_size = 0;
  for (int i = 0; i < doc_batch->GetNumDocs(); ++i) {
    LDADoc* doc_ptr = doc_batch->GetDocPtr(i);
    total_size += doc_ptr->SerializedSize();
  }
  if (buf_size_ < total_size) {
    delete[] mem_buf_;
    buf_size_ = total_size;
    mem_buf_ = new uint8_t[buf_size_];
  }
  std::vector<int32_t> doc_ids(doc_batch->GetNumDocs());
  uint8_t* mem_buf_tmp = mem_buf_;
  for (int i = 0; i < doc_batch->GetNumDocs(); ++i) {
    doc_ids[i] = doc_batch->GetDocID(i);
    LDADoc* doc_ptr = doc_batch->GetDocPtr(i);
    size_t serialized_size = doc_ptr->SerializedSize();
    doc_ptr->Serialize(mem_buf_tmp);
    leveldb::Slice key(
        reinterpret_cast<const char*>(doc_ids.data() + i),
        sizeof(int32_t));
    leveldb::Slice value(reinterpret_cast<const char*>(mem_buf_tmp),
        serialized_size);
    mem_buf_tmp += serialized_size;
    batch.Put(key, value);
    delete doc_ptr;   // doc is already serialized to mem_buf_.
  }
  leveldb::Status s = db_->Write(leveldb::WriteOptions(), &batch);
  CHECK(s.ok()) << "Batch write failed: " << s.ToString();
  delete doc_batch;
}

void DiskStreamReader::ClearOutstandingBatch(int32_t partition_id) {
  while (outstanding_batch_[partition_id]) {
    zmq::message_t zmq_msg;
    int32_t sender_id;
    doc_comm_bus_->RecvInProc(&sender_id, &zmq_msg);
    outstanding_batch_[sender_id] = false;
    DocBatch* doc_batch = *reinterpret_cast<DocBatch**>(zmq_msg.data());

    // This deletes doc_batch and all documents in it.
    BatchWriteAndDeleteDocs(doc_batch);
  }
}

void DiskStreamReader::CreateDocBatch(int32_t num_docs, DocBatch* doc_batch) {
  for (int i = 0; i < num_docs; ++i) {
    CHECK(it_->Valid());
    int32_t doc_id  = *reinterpret_cast<const int32_t*>(it_->key().data());
    CHECK_EQ(doc_id_begin_ + doc_counter_, doc_id);
    const uint8_t *doc_data
      = reinterpret_cast<const uint8_t*>(it_->value().data());
    LDADoc* new_doc = new LDADoc();
    new_doc->Deserialize(doc_data);
    doc_batch->AddDoc(doc_id, new_doc);
    ++doc_counter_;
    it_->Next();
  }
}

}  // namespace lda
