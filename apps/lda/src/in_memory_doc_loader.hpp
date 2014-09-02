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
// Date: 2014.06.18

#pragma once

#include "lda_doc.hpp"
#include "abstract_doc_loader.hpp"
#include "ldb_comparator.hpp"
#include <leveldb/db.h>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace lda {

// InMemoryMasterDocLoader loads the entire corpus in memory at the beginning
// before creating InMemoryPartitionDocLoader.
class InMemoryMasterDocLoader : public AbstractMasterDocLoader {
public:
  InMemoryMasterDocLoader(const std::string& doc_file,
      int32_t num_partitions, int32_t num_topics);

  // See AbstractMasterDocLoader. This method is thread-safe.
  AbstractPartitionDocLoader* GetPartitionDocLoader(int partition_id);

  ~InMemoryMasterDocLoader();

private:
  // These pointers (num_partitions_ of them) will eventually be transfered
  // to InMemoryPartitionDocLoader.
  std::vector<std::vector<LDADoc>* > doc_partitions_ptr_;
};


// Read all the documents into memory (as opposed to streaming from the disk).
class InMemoryPartitionDocLoader : public AbstractPartitionDocLoader {
public:
  // This takes the ownership of partition_docs. partition_docs are already
  // randomly initialized so no need for num_topics.
  InMemoryPartitionDocLoader(int32_t partition_id,
      std::vector<LDADoc>* partition_docs);

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

private:
  std::unique_ptr<std::vector<LDADoc> > partition_docs_;
  std::vector<LDADoc>::iterator curr_;
  int num_tokens_;
};

}  // namespace lda
