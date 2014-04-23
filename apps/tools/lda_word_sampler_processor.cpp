// Copyright (c) 2013, SailingLab
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

// Author: Dai Wei
// Date: 2013.12.13

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <algorithm>    // std::random_shuffle
#include <time.h>

// All these are required command line inputs
DEFINE_string(data_file, " ", "path to doc file in libsvm format.");
//DEFINE_string(vocab_file, " ", "path to vocab to int mapping.");
DEFINE_string(output_file, " ",
    "Results are in output_file.0, output_file.1, etc.");
DEFINE_int32(num_partitions, 0, "number of chunks to spit out");

typedef int32_t doc_id_t;
typedef int32_t token_count_t;
typedef std::pair<doc_id_t, token_count_t> doc_token_pair;

// random generator function:
int myrandom (int i) { return rand() % i;}

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  if (FLAGS_data_file == " " || FLAGS_output_file == " "
      || FLAGS_num_partitions == 0) {
    LOG(FATAL)
      << "usage: need data_file, vocab_file, output_file, and num_partitions";
  }

  // vocab_occur is a 2D array.
  std::vector<std::vector<doc_token_pair> > vocab_occur;

  char *line = NULL, *ptr = NULL, *endptr = NULL;
  size_t num_bytes;
  FILE *data_stream = fopen(FLAGS_data_file.c_str(), "r");
  CHECK_NOTNULL(data_stream);
  LOG(INFO) << "Reading from data file " << FLAGS_data_file;
  int doc_id = 0;
  int32_t word_id, count;
  int base = 10;
  while (getline(&line, &num_bytes, data_stream) != -1) {
    strtol(line, &endptr, base); // ignore first field (category label)
    ptr = endptr;
    while (*ptr != '\n') {
      while (*ptr != ' ') ++ptr; // goto next space
      // read a word_id:count pair
      word_id = strtol(++ptr, &endptr, base);
      ptr = endptr; // colon
      count = strtol(++ptr, &endptr, base);
      ptr = endptr;
      if (word_id >= vocab_occur.size()) vocab_occur.resize(word_id + 1);
      vocab_occur[word_id].push_back(std::make_pair(doc_id, count));
      while (*ptr != ' ' && *ptr != '\n') ++ptr; // goto next space or \n
    }
    ++doc_id;
    LOG_IF(INFO, doc_id % 200000 == 0) << "Reading doc " << doc_id;
  }
  free(line);
  CHECK_EQ(0, fclose(data_stream)) << "Failed to close file " << FLAGS_data_file;

  // active_vocabs are indices of vocabs that has at least 1 occurrence in the
  // corpus
  std::vector<int32_t> active_vocabs;
  int32_t num_tokens = 0;   // number of tokens globally.
  for (int i = 0; i < vocab_occur.size(); ++i) {
    std::vector<doc_token_pair> doc_token_pairs = vocab_occur[i];
    if (doc_token_pairs.size() > 0) {
      // skip vocab with 0 ocurrence.
      active_vocabs.push_back(i);
      for (auto& pair : doc_token_pairs) {
        num_tokens += pair.second;
      }
    }
  }
  int num_tokens_per_partition = static_cast<int>(num_tokens
      / FLAGS_num_partitions);

  // Do a random shuffling so each partition with the same number of tokens
  // will have roughly the same number of vocabs.
  srand(time(NULL));
  std::random_shuffle(active_vocabs.begin(), active_vocabs.end(), myrandom);

  LOG(INFO) << "Read " << vocab_occur.size() << " vocabs from "
    << doc_id << " docs. After filtering out "
    << (vocab_occur.size() - active_vocabs.size())
    << " zero occurrence words, we have "
    << active_vocabs.size() << " vocabs, " << num_tokens << " tokens."
    << " Each partition should have roughly " << num_tokens_per_partition
    << " tokens.";

  // Write to output_file (by partition)
  // new word mapping (from 0 to active_vocabs.size() - 1).
  int new_word_id = 0;
  for (int i = 0; i < FLAGS_num_partitions; ++i) {
    std::string filename(FLAGS_output_file);
    filename += "." + std::to_string(i);
    FILE *output_stream = fopen(filename.c_str(), "w");
    CHECK_NOTNULL(output_stream);
    // NOTE: Use the first line to record global number of docs
    fprintf(output_stream, "%d\n", doc_id);
    int num_tokens_count = 0;
    while (new_word_id < active_vocabs.size()) {
      if (num_tokens_count >= num_tokens_per_partition * .999
          && i != FLAGS_num_partitions - 1)
        break;
      fprintf(output_stream, "%d", new_word_id);   // word_id
      std::vector<doc_token_pair> doc_token_pairs =
        vocab_occur[active_vocabs[new_word_id]];
      for (auto& doc_token_pair : doc_token_pairs) {
        fprintf(output_stream, " %d:%d", doc_token_pair.first,
            doc_token_pair.second);
        num_tokens_count += doc_token_pair.second;
      }
      fprintf(output_stream, "\n");
      ++new_word_id;
    }
    CHECK_EQ(0, fclose(output_stream)) << "Failed to close file"
      << FLAGS_output_file;
    LOG(INFO) << "Wrote " << num_tokens_count
      << " tokens to output file " << filename;
  }

  // Write .map file, which has line format:
  //    data_file_vocab_idx  output_file_vocab_idx
  std::string filename(FLAGS_output_file);
  filename += ".map";
  FILE *output_stream = fopen(filename.c_str(), "w");
  CHECK_NOTNULL(output_stream);
  for (int i = 0; i < active_vocabs.size(); ++i) {
    fprintf(output_stream, "%d %d\n", active_vocabs[i], i);
  }
  CHECK_EQ(0, fclose(output_stream)) << "Failed to close file"
    << FLAGS_output_file;
  LOG(INFO) << "Wrote to vocab file " << filename;

  LOG(INFO) << "Done.";
  return 0;
}
