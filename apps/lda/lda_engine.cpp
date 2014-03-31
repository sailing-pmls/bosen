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
// Date: 2014.03.29

#include "lda_engine.hpp"
#include "context.hpp"
#include <cstdint>
#include <string>
#include <cstdlib>
#include <time.h>

namespace lda {

LDAEngine::LDAEngine() : thread_counter_(0), gen_(time(NULL)) {
  Context& context = Context::get_instance();
  K_ = context.get_int32("num_topics");
  one_K_dist_.reset(new boost::uniform_int<>(1, K_));
  one_K_rng_.reset(new rng_t(gen_, *one_K_dist_));
  num_threads_ = context.get_int32("num_app_threads");
  //process_barrier_.reset(new boost::barrier(num_threads_));
}

void LDAEngine::Start(bool init_thread) {
  engine_thread_data_.reset(new LDAEngineThreadData(thread_counter_++));
  if (!init_thread) {
    // This is not the init thread. Register it.
    petuum::TableGroup::RegisterThread();
  }
  int32_t thread_id = engine_thread_data_->thread_id_;
  LOG(INFO) << "Thread " << thread_id << " starts LDA.";
  Context context = Context::get_instance();
  FastDocSampler& sampler = engine_thread_data_->sampler_;
  LDACorpus& doc_partition = thread_docs_[thread_id];
  int num_iterations = context.get_int32("num_iterations");
  for (int iter = 0; iter < num_iterations; ++iter) {
    int j = 0;
    for (auto& doc : doc_partition) {
      LOG(INFO) << "thread " << thread_id << " is sampling doc " << j++;
      sampler.SampleOneDoc(&doc);
    }
    petuum::TableGroup::Clock();
  }
  if (!init_thread) {
    petuum::TableGroup::DeregisterThread();
  }
  LOG(INFO) << "Thread " << thread_id << " finish LDA.";
}

void LDAEngine::AddWordTopics(DocumentWordTopicsPB* doc_word_topics, int32_t word,
    int32_t num_tokens) {
  std::vector<int32_t> rand_topics(num_tokens);
  for (int32_t& t : rand_topics) {
    t = (*one_K_rng_)();
  }
  doc_word_topics->add_wordtopics(word, rand_topics);
}

void LDAEngine::ReadData(const std::string& doc_file) {
  char *line = NULL, *endptr = NULL;
  size_t num_bytes;
  int base = 10;
  FILE *doc_fp = fopen(doc_file.c_str(), "r");
  CHECK_NOTNULL(doc_fp);
  int num_docs = 0;
  LDACorpus pre_split_corpus;
  while (getline(&line, &num_bytes, doc_fp) != -1) {
    DocumentWordTopicsPB doc_word_topics;
    strtol(line, &endptr, base); // ignore first field (category label)
    char *ptr = endptr;
    while (*ptr != '\n') {
      while (*ptr != ' ') ++ptr; // goto next space
      int word_id = strtol(++ptr, &endptr, base);
      ptr = endptr; // colon
      int count = strtol(++ptr, &endptr, base);
      ptr = endptr;
      AddWordTopics(&doc_word_topics, word_id, count);
      while (*ptr != ' ' && *ptr != '\n') ++ptr; // goto next space or \n
    }
    LDADocument lda_doc(doc_word_topics);
    pre_split_corpus.push_back(lda_doc);
    ++num_docs;
  }
  free(line);
  CHECK_EQ(0, fclose(doc_fp)) << "Failed to close file " << doc_file;

  // Split the pre_split_corpus (in place) to num_threads_ partitions.
  thread_docs_.resize(num_threads_);
  LDACorpus::iterator splice_begin = pre_split_corpus.begin(), splice_end;
  int32_t docs_per_thread = static_cast<int>(num_docs / num_threads_);
  LOG(INFO) << "Read " << num_docs << " documents. Splitting into "
    << num_threads_ << " partitions. Each partition should have "
    << docs_per_thread << " docs";
  for (int thread_id = 0; thread_id < num_threads_; ++thread_id) {
    // Figure out splice_end.
    if (thread_id != num_threads_ - 1) {
      splice_end = splice_begin;
      for (int i = 0; i < docs_per_thread - 1; ++i) {
        ++splice_end;
      }
    } else {
      // The last thread takes the remainder docs.
      splice_end = pre_split_corpus.end();
    }
    LDACorpus& curr_corpus = thread_docs_[thread_id];
    curr_corpus.splice(curr_corpus.begin(), pre_split_corpus,
        splice_begin, splice_end);
    splice_begin = ++splice_end;
  }
}

}   // namespace lda
