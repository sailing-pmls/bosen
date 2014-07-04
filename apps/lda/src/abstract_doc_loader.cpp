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

#include "abstract_doc_loader.hpp"
#include <leveldb/db.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <glog/logging.h>

namespace lda {

// ======== AbstractMasterDocLoader ========

AbstractMasterDocLoader::AbstractMasterDocLoader(
    const std::string& doc_file, int32_t num_partitions, int32_t num_topics) :
  num_topics_(num_topics), num_partitions_(num_partitions),
  partition_doc_id_begin_(num_partitions),
  partition_doc_id_end_(num_partitions) {
    // Load the meta-data
    std::stringstream ss;
    ss << doc_file << ".meta";
    std::ifstream metadata_stream(ss.str().c_str());
    CHECK(metadata_stream) << "Failed to open meta-data file " << doc_file;
    std::string field_name;
    LOG(INFO) << "AbstractMasterDocLoader: Loading meta-data from "
      << ss.str();

    metadata_stream >> field_name >> max_vocab_id_;
    CHECK_EQ("max_vocab_id:", field_name);

    metadata_stream >> field_name >> num_vocabs_;
    CHECK_EQ("num_vocabs:", field_name);

    metadata_stream >> field_name >> num_docs_;
    CHECK_EQ("num_docs:", field_name);

    metadata_stream >> field_name >> doc_id_begin_;
    CHECK_EQ("doc_id_begin:", field_name);

    metadata_stream >> field_name >> doc_id_end_;
    CHECK_EQ("doc_id_end:", field_name);

    metadata_stream.close();

    // Compute the document range for each partition.
    int32_t num_docs_per_partition = num_docs_ / num_partitions;
    for (int i = 0; i < num_partitions; ++i) {
      partition_doc_id_begin_[i] = i * num_docs_per_partition + doc_id_begin_;
      if (i == num_partitions - 1) {
        partition_doc_id_end_[i] = doc_id_end_;
      } else {
        partition_doc_id_end_[i] = partition_doc_id_begin_[i]
          + num_docs_per_partition;
      }
    }
  }

AbstractMasterDocLoader::~AbstractMasterDocLoader() { }


// ============ AbstractPartitionDocLoader ==============

AbstractPartitionDocLoader::AbstractPartitionDocLoader(
    int32_t partition_id) :
  partition_id_(partition_id) { }

AbstractPartitionDocLoader::~AbstractPartitionDocLoader() {}

}  // namespace lda
