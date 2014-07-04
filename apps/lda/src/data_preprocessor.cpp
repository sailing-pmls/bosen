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
// Date: 2014.04.03

#include "lda_doc.hpp"
#include "ldb_comparator.hpp"
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <algorithm>    // std::random_shuffle
#include <time.h>
#include <fstream>
#include <set>
#include <random>
#include <ctype.h>
#include <sstream>
#include <leveldb/db.h>
#include <boost/unordered_map.hpp>


// All these are required command line inputs
DEFINE_string(data_file, " ", "path to doc file in libsvm format.");
DEFINE_string(output_file, " ",
    "Results are in output_file.0, output_file.1, etc."
    "Meta files are in output_file.0.meta, etc");
DEFINE_int32(num_partitions, 0, "number of chunks to spit out");

// random generator function:
int MyRandom (int i) {
  static std::default_random_engine e;
  return e() % i;
}

std::string MakePartitionDBPath(int32_t partition) {
  std::stringstream ss;
  ss << FLAGS_output_file;
  ss << "." << partition;
  return ss.str();
}

std::string MakePartitionMetaDBPath(int32_t partition) {
  std::stringstream ss;
  ss << FLAGS_output_file;
  ss << "." << partition << ".meta";
  return ss.str();
}

std::vector<lda::LDADoc> corpus;
boost::unordered_map<int32_t, bool> vocab_occur;

int max_word_id = 0;
const int base = 10;

int num_tokens = 0;

std::vector<leveldb::DB*> db_vector;

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  char *line = NULL, *ptr = NULL, *endptr = NULL;
  size_t num_bytes;

  FILE *data_stream = fopen(FLAGS_data_file.c_str(), "r");
  CHECK_NOTNULL(data_stream);
  LOG(INFO) << "Reading from data file " << FLAGS_data_file;
  int doc_id = 0;
  int32_t word_id, count;
  int num_vocabs_occur = 0;   // sum of # of vocabs in each doc.
  while (getline(&line, &num_bytes, data_stream) != -1) {
    lda::LDADoc new_doc;
    //new_doc.InitAppendWord();

    strtol(line, &endptr, base); // ignore first field (category label)
    ptr = endptr;

    while (*ptr != '\n') {
      // read a word_id:count pair
      word_id = strtol(ptr, &endptr, base);
      ptr = endptr;

      CHECK_EQ(':', *ptr) << "doc_id = " << doc_id;
      ++ptr;

      count = strtol(ptr, &endptr, base);
      ptr = endptr;

      new_doc.AppendWord(word_id, count);

      max_word_id = std::max(word_id, max_word_id);
      vocab_occur[word_id] = true;

      num_tokens += count;
      ++num_vocabs_occur;

      while(isspace(*ptr) && (*ptr != '\n')) ++ptr;
    }

    if (new_doc.GetNumTokens() > 0) {
      corpus.push_back(new_doc);
    }
    ++doc_id;
    LOG_EVERY_N(INFO, 100000) << "Reading doc " << doc_id;
  }
  free(line);
  CHECK_EQ(0, fclose(data_stream)) << "Failed to close file "
    << FLAGS_data_file;

  int num_tokens_per_partition = static_cast<int>(num_tokens
      / FLAGS_num_partitions);

  // Do a random shuffling so each partition with the same number of tokens
  // will have roughly the same number of vocabs.
  srand(time(NULL));
  std::random_shuffle(corpus.begin(), corpus.end(), MyRandom);

  size_t num_tokens_per_doc = num_tokens / corpus.size();

  LOG(INFO) << "number of docs: " << corpus.size();
  LOG(INFO) << "vocab size: " << vocab_occur.size();
  LOG(INFO) << "number of tockens: " << num_tokens;
  LOG(INFO) << "number of tokens per partition: " << num_tokens_per_partition;
  LOG(INFO) << "number of tokens per doc: " << num_tokens_per_doc;
  LOG(INFO) << "max_word_id: " << max_word_id;
  LOG(INFO) << "avg repeation of a vocab in a doc: "
    << static_cast<float>(num_tokens) / num_vocabs_occur;

  db_vector.resize(FLAGS_num_partitions);
  lda::IntComparator cmp;   // must be alive throughout the lifetime of db.
  for (int i = 0; i < FLAGS_num_partitions; ++i) {
    leveldb::Options options;
    options.create_if_missing = true;
    options.error_if_exists = true;
    options.block_size = 1024*1024;
    options.comparator = &cmp;
    std::string db_path = MakePartitionDBPath(i);
    leveldb::Status db_status = leveldb::DB::Open(options, db_path,
        &(db_vector[i]));
    CHECK(db_status.ok()) << "open leveldb failed, partition = " << i;
  }

  size_t num_tokens_curr_partition = 0;
  int32_t curr_partition = 0;
  size_t buf_size = 1024;
  uint8_t *mem_buf = new uint8_t[buf_size];
  int32_t curr_partition_idx_start = 0;   // the first doc index of this partition.
  size_t partition_data_size = 0;
  for (int32_t doc_idx = 0; doc_idx < corpus.size(); ++doc_idx) {
    size_t serialized_size = corpus[doc_idx].SerializedSize();
    if (buf_size < serialized_size) {
      delete[] mem_buf;
      buf_size = serialized_size;
      mem_buf = new uint8_t[buf_size];
    }

    partition_data_size += serialized_size;

    corpus[doc_idx].Serialize(mem_buf);
    leveldb::Slice key(reinterpret_cast<const char*>(&doc_idx),
        sizeof(doc_idx));

    leveldb::Slice value(reinterpret_cast<const char*>(mem_buf),
        serialized_size);

    db_vector[curr_partition]->Put(leveldb::WriteOptions(), key, value);
    num_tokens_curr_partition += corpus[doc_idx].GetNumTokens();

    if (((curr_partition < FLAGS_num_partitions - 1)
        && (num_tokens_curr_partition
          > (num_tokens_per_partition - num_tokens_per_doc))) ||
        // Last partition takes all tokens.
        (curr_partition == FLAGS_num_partitions - 1
         && doc_idx == corpus.size() - 1)) {
      LOG(INFO) << "====== Partition " << curr_partition << " =====";
      LOG(INFO) << "filename: " << MakePartitionDBPath(curr_partition);
      LOG(INFO) << "number tokens: " << num_tokens_curr_partition;
      int num_docs_this_partition = doc_idx - curr_partition_idx_start + 1;
      LOG(INFO) << "number docs: " << num_docs_this_partition;
      LOG(INFO) << "partition bytes: " << partition_data_size;
      partition_data_size = 0;
      num_tokens_curr_partition = 0;

      // Writing meta file.
      std::ofstream meta_file;
      meta_file.open(MakePartitionMetaDBPath(curr_partition));
      meta_file << "max_vocab_id: " << max_word_id << std::endl
        << "num_vocabs: " << vocab_occur.size() << std::endl
        << "num_docs: " << num_docs_this_partition << std::endl
        << "doc_id_begin: " << curr_partition_idx_start << std::endl
        << "doc_id_end: " << doc_idx + 1;
      meta_file.close();
      curr_partition_idx_start = doc_idx + 1;
      ++curr_partition;
    }
  }

  delete[] mem_buf;
  for (int i = 0; i < FLAGS_num_partitions; ++i) {
    delete db_vector[i];
  }

  return 0;
}
