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
#include "ldb_comparator.hpp"
#include <leveldb/db.h>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace lda {

class AbstractPartitionDocLoader;

// AbstractMasterDocLoader generates PartitionDocLoader.
class AbstractMasterDocLoader {
public:
  // filename is the path to the LevelDB directory. There should also be a
  // meta-data file called filename.meta. This constructor reads the meta
  // data. num_topics is needed to randomly initialize topics.
  AbstractMasterDocLoader(const std::string& doc_file, int32_t num_partitions,
      int32_t num_topics);

  virtual ~AbstractMasterDocLoader();

  // Generate respective PartitionDocLoader. It can only be called once for
  // each partition_id. MasterDocLoader does not take ownership of the
  // any data in the engendered PartitionDocLoader. This method is
  // thread-safe.
  virtual AbstractPartitionDocLoader* GetPartitionDocLoader(int partition_id)
    = 0;

  // # of docs handled by this data loader.
  inline int32_t GetNumDocs() const {
    return num_docs_;
  }

  inline int32_t GetMaxVocabID() const {
    return max_vocab_id_;
  }

  inline int32_t GetNumVocabs() const {
    return num_vocabs_;
  }

protected:
  int32_t num_topics_;
  int32_t num_partitions_;
  int32_t max_vocab_id_;  // across entire corpus (from meta-data)
  int32_t num_vocabs_;    // across entire corpus (from meta-data)
  int32_t num_docs_;
  int32_t doc_id_begin_;
  int32_t doc_id_end_;

  // Global document ID range of each partition.
  std::vector<int32_t> partition_doc_id_begin_;
  std::vector<int32_t> partition_doc_id_end_;

  // comparator for leveldb.
  IntComparator cmp_;
};

// Specifies interface for loading documents in a partition.
class AbstractPartitionDocLoader {
public:
  AbstractPartitionDocLoader(int32_t partition_id);
  virtual ~AbstractPartitionDocLoader();

  // # of documents in this partition.
  virtual int GetNumDocs() const  = 0;

  // This function will only return the correct # of tokens after all documents
  // in the partition has been read once.
  virtual int GetNumTokens() const = 0;

  // GetOneDoc, IsEnd, and Restart() together constitute poor man's iterator
  // over a doc partition. The returned doc will have topics initialized
  // (randomly or stored from previous runs).
  //
  // Get the current document and advance the iterator. The returned pointer
  // remains valid till the next GetOneDoc call. The behavior of GetOneDoc()
  // is undefined if IsEnd() is true.
  virtual LDADoc* GetOneDoc() = 0;

  // End of a data sweep.
  virtual bool IsEnd() const = 0;

  // GetOneDoc() will return the first document after Restart().
  virtual void Restart() = 0;

protected:
  int32_t partition_id_;
};

}  // namespace lda
