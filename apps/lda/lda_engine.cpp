// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.03.29

#include "lda_engine.hpp"
#include "utils.hpp"
#include "high_resolution_timer.hpp"
#include "context.hpp"
#include <unordered_map>
#include <cstdint>
#include <string>
#include <cstdlib>
#include <time.h>
#include <glog/logging.h>
#include <mutex>
#include <set>

namespace lda {

const int32_t LDAEngine::kInverseContentionProb = 2;

LDAEngine::LDAEngine() : thread_counter_(0), gen_(time(NULL)) {
  util::Context& context = util::Context::get_instance();
  K_ = context.get_int32("num_topics");
  one_K_dist_.reset(new boost::uniform_int<>(1, K_));
  one_K_rng_.reset(new rng_t(gen_, *one_K_dist_));
  num_threads_ = context.get_int32("num_worker_threads");
  compute_ll_interval_ = context.get_int32("compute_ll_interval");
  process_barrier_.reset(new boost::barrier(num_threads_));
}

void LDAEngine::Start() {
  petuum::TableGroup::RegisterThread();

  // Initialize local thread data structures.
  int thread_id = thread_counter_++;
  FastDocSampler sampler;
  LDAStats lda_stats;
  //LOG(INFO) << "Thread " << thread_id << " starts LDA.";

  util::Context& context = util::Context::get_instance();
  int client_id = context.get_int32("client_id");

  int32_t summary_table_id = context.get_int32("summary_table_id");
  int32_t word_topic_table_id = context.get_int32("word_topic_table_id");
  petuum::Table<int> summary_table =
    petuum::TableGroup::GetTableOrDie<int>(summary_table_id);
  petuum::Table<int> word_topic_table =
    petuum::TableGroup::GetTableOrDie<int>(word_topic_table_id);
  docs_iter_ = docs_.begin();
  process_barrier_->wait();

  // Tally the word-topic counts for this thread. Use batch to reduce row
  // contentions.
  petuum::UpdateBatch<int> summary_updates;
  std::unordered_map<int, petuum::UpdateBatch<int> > word_topic_updates;
  while (true) {
    auto doc_iter = GetOneDoc();
    if (doc_iter == docs_.end()) {
      break;
    }
    for (LDADocument::WordOccurrenceIterator it(&(*doc_iter));
        !it.Done(); it.Next()) {
      word_topic_updates[it.Word()].Update(it.Topic(), 1);
      summary_updates.Update(it.Topic(), 1);
    }
  }
  for (auto it = word_topic_updates.begin();
      it != word_topic_updates.end(); ++it) {
    word_topic_table.BatchInc(it->first, it->second);
  }
  summary_table.BatchInc(0, summary_updates);

  petuum::TableGroup::GlobalBarrier();

  if (thread_id == 0) {
    LOG(INFO) << "Done initializing topic assignments (uniform random).";
    util::HighResolutionTimer bootstrap_timer;
    for (auto it = local_vocabs_.begin(); it != local_vocabs_.end(); ++it) {
      word_topic_table.GetAsync(*it);
    }
    word_topic_table.WaitPendingAsyncGet();
    double seconds_bootstrap = bootstrap_timer.elapsed();
    LOG(INFO) << "Bootstrap time = " << seconds_bootstrap << " seconds.";
  }

  process_barrier_->wait();
  int num_iterations = context.get_int32("num_iterations");
  util::HighResolutionTimer total_timer;
  for (int iter = 1; iter <= num_iterations; ++iter) {
    docs_iter_ = docs_.begin();
    num_tokens_clock_ = 0;
    process_barrier_->wait();

    // Sampling.
    util::HighResolutionTimer iter_timer;
    // Refresh summary row cache every iteration.
    sampler.RefreshCachedSummaryRow();
    int num_docs_this_iter = 0;
    while (true) {
      auto doc_iter = GetOneDoc();
      if (doc_iter == docs_.end()) break;
      sampler.SampleOneDoc(&(*doc_iter));
      num_tokens_clock_ += doc_iter->num_tokens();
      ++num_docs_this_iter;
      if (num_docs_this_iter % (num_threads_ * kInverseContentionProb) == 0) {
        // Occasionally refresh summary row.
        sampler.RefreshCachedSummaryRow();
      }
    }
    petuum::TableGroup::Clock();

    if (thread_id == 0) {
      double seconds_this_iter = iter_timer.elapsed();
      LOG(INFO)
        << "Iter: " << iter
        << "\tClient " << client_id
        << "\tTook: " << seconds_this_iter << " sec"
        << "\tThroughput: "
        << static_cast<double>(num_tokens_clock_) /
        num_threads_ / seconds_this_iter << " token/(thread*sec)";
    }

    // Barrier to prevent another thread run ahead and set num_tokens_clock_
    // to 0.
    process_barrier_->wait();

    // Compute LLH.
    if (compute_ll_interval_ != -1 && iter % compute_ll_interval_ == 0) {
      // Every thread compute doc-topic LLH component.
      int ith_llh = iter / compute_ll_interval_;
      docs_iter_ = docs_.begin();
      process_barrier_->wait();
      while (true) {
        auto doc_iter = GetOneDoc();
        if (doc_iter == docs_.end()) break;
        lda_stats.ComputeOneDocLLH(ith_llh, &(*doc_iter));
      }
      // Each client take turn to compute word LLH.
      int num_clients = context.get_int32("num_clients");
      if (ith_llh % num_clients == client_id && thread_id == 0) {
        lda_stats.ComputeWordLLH(ith_llh, iter);
        // print LLH up to last iteration (for real-time tracking).
        int prev_llh_idx = ith_llh - 1;
        if (prev_llh_idx >= 0) {
          LOG(INFO) << "client " << client_id << " at iteration " << iter << " LLH: "
            << lda_stats.PrintOneLLH(prev_llh_idx);
        }
      }
    }
  }   // for iter.

  petuum::TableGroup::GlobalBarrier();
  // Head thread print out the LLH.
  if (client_id == 0 && thread_id == 0) {
    int num_llh = num_iterations / compute_ll_interval_;
    LOG(INFO) << "Total time for " << num_iterations << " iterations : "
      << total_timer.elapsed() << " sec.";
    LOG(INFO) << "\n" << lda_stats.PrintLLH(num_llh);
  }

  petuum::TableGroup::DeregisterThread();
}

LDACorpus::iterator LDAEngine::GetOneDoc() {
  std::unique_lock<std::mutex> ulock(docs_mtx_);
  if (docs_iter_ == docs_.end()) {
    return docs_.end();
  }
  return docs_iter_++;
}

void LDAEngine::AddWordTopics(DocumentWordTopicsPB* doc_word_topics, int32_t word,
    int32_t num_tokens) {
  std::vector<int32_t> rand_topics(num_tokens);
  for (int32_t& t : rand_topics) {
    t = (*one_K_rng_)() - 1;
  }
  doc_word_topics->add_wordtopics(word, rand_topics);
}

int32_t LDAEngine::ReadData(const std::string& doc_file) {
  char *line = NULL, *endptr = NULL;
  size_t num_bytes;
  int base = 10;
  FILE *doc_fp = fopen(doc_file.c_str(), "r");
  CHECK_NOTNULL(doc_fp);
  int num_docs = 0;
  while (getline(&line, &num_bytes, doc_fp) != -1) {
    DocumentWordTopicsPB doc_word_topics;
    strtol(line, &endptr, base); // ignore first field (category label)
    char *ptr = endptr;
    while (*ptr == ' ') ++ptr; // goto next non-space char
    while (*ptr != '\n') {
      // read a word_id:count pair
      int32_t word_id = strtol(ptr, &endptr, base);
      ptr = endptr; // *ptr = colon
      CHECK_EQ(':', *ptr) << "num_docs = " << num_docs;
      int32_t count = strtol(++ptr, &endptr, base);
      ptr = endptr;
      AddWordTopics(&doc_word_topics, word_id, count);
      local_vocabs_.insert(word_id);
      while (*ptr == ' ') ++ptr; // goto next non-space char
    }
    LDADocument lda_doc(doc_word_topics);
    docs_.push_back(lda_doc);
    ++num_docs;
  }
  free(line);
  CHECK_EQ(0, fclose(doc_fp)) << "Failed to close file " << doc_file;

  int32_t docs_per_thread = static_cast<int>(num_docs / num_threads_);
  LOG(INFO) << "Read " << num_docs << " documents which uses "
    << local_vocabs_.size()
    << " local_vocabs_; has " << num_threads_
    << " threads. Each thread roughly has "
    << docs_per_thread << " docs";
  return local_vocabs_.size();
}

}   // namespace lda
