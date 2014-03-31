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

#include "lda_sampler.hpp"
#include "context.hpp"
#include "types.hpp"
#include "utils.hpp"

#include <petuum_ps/include/petuum_ps.hpp>
#include <glog/logging.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <queue>

namespace lda {

namespace {

#define LOG_WHO LOG_IF(INFO, sampler_thread_data_->thread_id == 0) << "client_" << client_id << ") "

struct comp {
  bool operator()(wcmap_t a, wcmap_t b) {
    return (a.count < b.count);
  }
};

}   // anonymous namespace

LDASampler::LDASamplerThreadData::LDASamplerThreadData()
  : generator_(time(NULL)), uniform_zero_one_dist_(0, 1) {
  zero_one_generator_.reset(new rng_t(generator_, uniform_zero_one_dist_));
}

LDASampler::LDASampler() : num_words_initialized_(0), thread_counter_(0) {
  Context& context = Context::get_instance();
  int summary_table_id = context.get_int32("summary_table_id");
  summary_table_ = petuum::TableGroup::GetTableOrDie<int>(summary_table_id);
  int doc_topic_table_id = context.get_int32("doc_topic_table_id");
  doc_topic_table_ = petuum::TableGroup::GetTableOrDie<int>(doc_topic_table_id);

  // Initialize process barrier.
  int32_t num_threads = context.get_int32("num_worker_threads");
  CHECK_GT(num_threads, 0)
    << "A sampler needs to have >0 number of worker threads.";
  process_barrier_.reset(new boost::barrier(num_threads));

  num_iterations_ = context.get_int32("num_iterations");
  compute_ll_interval_ = context.get_int32("compute_ll_interval");
}

void LDASampler::ReadData(bool zero_indexed) {
  Context& context = Context::get_instance();
  int32_t num_topics = context.get_int32("num_topics");
  double beta = context.get_double("beta");
  double alpha = context.get_double("alpha");
  std::string data_file = context.get_string("data_file");
  std::string vocab_file = context.get_string("vocab_file");

  // Prepare for stdio
  char *line = NULL, *ptr = NULL;
  size_t num_bytes;
  int32_t word_id, doc_id, count;

  // Count number of words from vocabulary file. 
  // Construct dict_.
  FILE *vocab_stream = fopen(vocab_file.c_str(), "r");
  CHECK_NOTNULL(vocab_stream);
  LOG(INFO) << "Reading from word file = " << vocab_file;
  int32_t num_global_words = 0;
  char buff[256];
  while (getline(&line, &num_bytes, vocab_stream) != -1) {
    sscanf(line, "%s", buff);
    dict_.push_back(buff); // trust implicit cast...
    ++num_global_words;
  }
  CHECK_EQ(num_global_words, dict_.size());
  context.set("num_global_words", num_global_words);
  CHECK_EQ(fclose(vocab_stream), 0) << "Failed to close file " << vocab_file;
  LOG(INFO) << "Num of Global Words: " << num_global_words;

  // Read word stat file
  FILE *data_stream = fopen(data_file.c_str(), "r");
  CHECK_NOTNULL(data_stream);
  LOG(INFO) << "Reading from data file = " << data_file;

  // Use the first line to record global number of docs
  int num_global_docs;
  CHECK_NE(getline(&line, &num_bytes, data_stream), -1);
  sscanf(line, "%d", &num_global_docs);
  context.set("num_docs", num_global_docs);
  LOG(INFO) << "Num of Training Docs: " << num_global_docs;

  int32_t num_local_words = 0;
  while (getline(&line, &num_bytes, data_stream) != -1) {
    // stat of a word
    ptr = line; // point to the start
    sscanf(ptr, "%d", &word_id); // read word_id
    // Construct a word sampler
    word_samplers_[word_id].Init(word_id);
    while (*ptr != '\n') {
      while (*ptr != ' ') ++ptr; // goto next space
      // read a doc_id:count pair
      sscanf(++ptr, "%d:%d", &doc_id, &count);
      // Add stat to word sampler
      word_samplers_[word_id].AddDocTokens(doc_id, count);
      while (*ptr != ' ' && *ptr != '\n') ++ptr; // goto next space or \n
    }
    ++num_local_words;
  }
  free(line);
  context.set("num_local_words", num_local_words);
  CHECK_EQ(fclose(data_stream), 0) << "Failed to close file " << data_file;
  LOG(INFO) << "Num of Local Words: " << num_local_words;

  // Assuming symmetric Dirichlet prior
  context.set("beta_sum", beta * num_global_words);
  context.set("alpha_sum", alpha * num_topics);

  // Initialize word_iterator_ while it's still single threaded.
  word_iterator_ = word_samplers_.begin();
}

void LDASampler::RunSampler() {
  // Initialize thread specific data.
  if (!sampler_thread_data_.get()) {
    sampler_thread_data_.reset(new LDASamplerThreadData());
    // thread_counter++ is atomic.
    sampler_thread_data_->thread_id = thread_counter_++;
  }

  // Needed to collect per-thread stats.
  Context& context = Context::get_instance();
  int32_t num_threads = context.get_int32("num_worker_threads");
  bool head_client = context.get_bool("head_client");
  int32_t client_id = context.get_int32("client_id");

  // Register thread on PS.
  petuum::TableGroup::RegisterThread();

  // Initialize topics.
  LOG_WHO << "Initializing topics..";
  num_words_initialized_ += InitTopics();
  process_barrier_->wait();
  // Verify the initialization.
  if (sampler_thread_data_->thread_id == 0) {
    CHECK_EQ(word_samplers_.size(), num_words_initialized_)
      << "Num of processed words mismatch.";
  }

  // Flush the oplog and invalidate all caches.
  petuum::TableGroup::GlobalBarrier();

  LOG_WHO << "Start Collapsed Gibbs Sampling";

  // Main for loop
  for (int iter = 1; iter <= num_iterations_; ++iter) {
    num_tokens_iteration_ = 0;
    word_iterator_ = word_samplers_.begin();
    process_barrier_->wait();
    double t1 = 0;
    if (sampler_thread_data_->thread_id == 0) {
      t1 = get_time();
    }

    SampleOneIteration();
    petuum::TableGroup::Clock();

    // Compute likelihood
    process_barrier_->wait();
    if (iter % compute_ll_interval_ == 0 || iter == num_iterations_) {
      ComputeLocalWordLikelihood();
      LOG_WHO
          << "Iter: " << iter << " likelihood_part_1 " << word_likelihood_;
      if (head_client) {
        ComputeDocLikelihood();
        LOG_WHO
            << "Iter: " << iter << " likelihood_part_2 " << doc_likelihood_;
      }
    }

    // Barriers are used to collect stats.
    process_barrier_->wait();
    double seconds_this_iter;
    if (sampler_thread_data_->thread_id == 0) {
      seconds_this_iter = get_time() - t1;
      LOG_WHO
        << "Iter: " << iter
        << "\tTook: " << seconds_this_iter << " sec"
        << "\tThroughput: "
        << static_cast<double>(num_tokens_iteration_) /
        num_threads / seconds_this_iter << " token/(thread*sec)";
    }

    /*
    if (iter == num_iterations_ - 1 &&
        sampler_thread_data_->thread_id == 0) {
      PrintTopWords();
    }
    */
  }

  // Flush the oplog and invalidate all caches.
  petuum::TableGroup::GlobalBarrier();

  // dump result
  if (context.get_string("output_prefix") != "" &&
      sampler_thread_data_->thread_id == 0) {
    Dump();
  }

  petuum::TableGroup::DeregisterThread();
}

// ==================== Private Methods ===================

int32_t LDASampler::InitTopics() {
  int32_t num_words_initialized = 0;
  while (true) {
    int32_t word_id = 0;

    // Enter critical region
    {
      boost::lock_guard<boost::mutex> guard(word_lock_);
      if (word_iterator_ == word_samplers_.end()) {
        break;
      } else {
        // Safely increment iterator
        word_id = word_iterator_->first;
        ++word_iterator_;
      }
    }

    // Do init topics
    word_samplers_[word_id].InitTopicsUniform();
    ++num_words_initialized;
  }
  return num_words_initialized;
}

void LDASampler::SampleOneIteration() {
  while (true) {
    int32_t word = 0;

    // Enter critical region
    {
      boost::lock_guard<boost::mutex> guard(word_lock_);
      if (word_iterator_ == word_samplers_.end()) {
        break;
      } else {
        // Safely increment iterator
        word = word_iterator_->first;
        ++word_iterator_;
      }
    }

    // Do sampling
    word_samplers_[word].Sample(
          sampler_thread_data_->zero_one_generator_.get());

    // Stats collection
    num_tokens_iteration_ += word_samplers_[word].GetNumTokens();
  }   // end of for or while loop.
}

void LDASampler::ComputeLocalWordLikelihood() {
  // Reset
  if (sampler_thread_data_->thread_id == 0) {
    word_likelihood_ = 0;
    word_iterator_ = word_samplers_.begin();
  }
  // Wait until reset is successfully done
  process_barrier_->wait();

  // Compute this_thread
  double this_thread = 0.0;
  while (true) {
    int32_t word_id = 0;
    {
      boost::lock_guard<boost::mutex> guard(word_lock_);
      if (word_iterator_ == word_samplers_.end()) {
        break;
      } else {
        // Safely increment iterator
        word_id = word_iterator_->first;
        ++word_iterator_;
      }
    }
    this_thread += word_samplers_[word_id].ComputeWordLikelihood();
  }

  // Safely sum up partials
  {
    boost::lock_guard<boost::mutex> guard(word_likelihood_lock_);
    word_likelihood_ += this_thread;
  }
}

void LDASampler::ComputeDocLikelihood() {
  Context& context = Context::get_instance();
  int32_t num_docs = context.get_int32("num_docs");
  int32_t num_topics = context.get_int32("num_topics");
  double alpha = context.get_double("alpha");
  double alpha_sum = context.get_double("alpha_sum");
  double beta_sum = context.get_double("beta_sum");

  // Reset
  if (sampler_thread_data_->thread_id == 0) {
    doc_likelihood_ = 0;
    doc_iterator_ = 0;
  }
  // Wait until reset is successfully done
  process_barrier_->wait();

  double this_thread = 0.0;
  while (true) {
    int32_t doc_id = 0;
    {
      boost::lock_guard<boost::mutex> guard(doc_lock_);
      if (doc_iterator_ >= num_docs) {
        CHECK_EQ(doc_iterator_, num_docs); // shouldn't be greater though
        break;
      } else {
        // Safely increment iterator
        doc_id = doc_iterator_++;
      }
    }
    
    // Compute one doc (not strictly one doc though)
    double one_doc = 0.0;
    int32_t doc_topic_count;
    int32_t num_non_zero = 0;
    int32_t doc_length = 0;
    petuum::RowAccessor doc_topic_row_acc;
    doc_topic_table_.Get(doc_id, &doc_topic_row_acc);
    const petuum::DenseRow<int32_t>& doc_topic_row =
      doc_topic_row_acc.Get<petuum::DenseRow<int32_t> >();
    for (int k = 0; k < num_topics; ++k) {
      doc_topic_count = doc_topic_row[k];
      if (doc_topic_count > 0) {
        one_doc += LogGamma(doc_topic_count + alpha);
        ++num_non_zero;
        doc_length += doc_topic_count;
      }
    }
    one_doc -= num_non_zero * LogGamma(alpha);
    one_doc -= LogGamma(alpha_sum + doc_length);

    // Add to this_thread
    this_thread += one_doc;
  }

  // Add constant component. Should be added at most once.
  if (sampler_thread_data_->thread_id == 0) {
    this_thread += num_docs * LogGamma(alpha_sum);
  }

  // Likelihood component that involves sum row.
  // Note that in the equation should have been part of word likelihood, however
  // it's here because it need to be computed only once.
  if (sampler_thread_data_->thread_id == 0) {
    int32_t sum_count;
    int32_t num_non_zero = 0;
    petuum::RowAccessor summary_row_acc;
    summary_table_.Get(0, &summary_row_acc);
    const petuum::DenseRow<int32_t>& summary_row =
      summary_row_acc.Get<petuum::DenseRow<int32_t> >();
    for (int k = 0; k < num_topics; ++k) {
      sum_count = summary_row[k];
      if (sum_count > 0) {
        this_thread -= LogGamma(sum_count + beta_sum);
        ++num_non_zero;
      }
    }
    this_thread += num_non_zero * LogGamma(beta_sum);
  }

  // Safely sum up partials
  {
    boost::lock_guard<boost::mutex> guard(doc_likelihood_lock_);
    doc_likelihood_ += this_thread;
  }
}

void LDASampler::Dump() {
  Context& context = Context::get_instance();
  int num_topics = context.get_int32("num_topics");
  std::string output_prefix = context.get_string("output_prefix");
  int32_t client_id = context.get_int32("client_id");
  bool head_client = context.get_bool("head_client");
  int32_t num_docs = context.get_int32("num_docs");
  int32_t num_global_words = context.get_int32("num_global_words");

  // dump word-topic counts
  std::string word_topic_dump = output_prefix +
    ".word_topic." +
    std::to_string(client_id);
  FILE *wt_stream = fopen(word_topic_dump.c_str(), "w");
  CHECK_NOTNULL(wt_stream);
  LOG_WHO << "Writing output to " << word_topic_dump << " ...";
  for (int word_id = 0; word_id < num_global_words; ++word_id) {
    auto it = word_samplers_.find(word_id);
    if (it != word_samplers_.end())
      fprintf(wt_stream, "%s", (it->second).Print().c_str());
  }
  CHECK_EQ(fclose(wt_stream), 0) << "Failed to close file " << word_topic_dump;

  // dump doc-topic counts
  if (head_client) {
    std::string doc_topic_dump = output_prefix + ".doc_topic";
    FILE *dt_stream = fopen(doc_topic_dump.c_str(), "w");
    CHECK_NOTNULL(dt_stream);
    LOG_WHO << "Writing output to " << doc_topic_dump << " ...";
    for (int doc_id = 0; doc_id < num_docs; ++doc_id) {
      fprintf(dt_stream, "%d", doc_id);
      int dtc;
      petuum::RowAccessor doc_topic_row_acc;
      doc_topic_table_.Get(doc_id, &doc_topic_row_acc);
      const petuum::DenseRow<int32_t>& doc_topic_row =
        doc_topic_row_acc.Get<petuum::DenseRow<int32_t> >();
      for (int k = 0; k < num_topics; ++k) {
        dtc = doc_topic_row[k];
        if (dtc > 0)
          fprintf(dt_stream, " %d:%d", k, dtc);
      }
      fprintf(dt_stream, "\n");
    }
    CHECK_EQ(fclose(dt_stream), 0) << "Failed to close file " << doc_topic_dump;
  }
}

void LDASampler::PrintTopWords() {
  Context& context = Context::get_instance();
  int num_topics = context.get_int32("num_topics");
  int num_top_words = context.get_int32("num_top_words");

  for (int k = 0; k < num_topics; ++k) {
    word_iterator_ = word_samplers_.begin();
    std::priority_queue<wcmap_t,
      std::vector<wcmap_t>,
      comp> top_words;
    for (; word_iterator_ != word_samplers_.end(); ++word_iterator_) {
      top_words.push((word_iterator_->second).GetWordCountMap(k));
    }
    std::stringstream ss;
    for (int i = 0; i < num_top_words; ++i) {
      int word = top_words.top().word;
      int count = top_words.top().count;
      ss << " " << dict_[word] << ":" << count;
      top_words.pop();
    }
    LOG(INFO) << "Topic " << k << "," << ss.str();
  }
}


}   // namespace lda
