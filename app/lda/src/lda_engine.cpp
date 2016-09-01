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
#include "in_memory_doc_loader.hpp"
#include "disk_stream_doc_loader.hpp"
#include "workload_manager.hpp"
#include "table_columns.hpp"
#include "utils.hpp"
#include "context.hpp"
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <cstdlib>
#include <time.h>
#include <glog/logging.h>
#include <mutex>
#include <set>
#include <petuum_ps_common/include/petuum_ps.hpp>
#include <leveldb/db.h>

DECLARE_int32(client_id);
DECLARE_int32(num_topics);
DECLARE_int32(summary_table_id);
DECLARE_int32(word_topic_table_id);
DECLARE_double(alpha);
DECLARE_double(beta);
DECLARE_string(output_file_prefix);
DECLARE_bool(output_topic_word);
DECLARE_bool(output_doc_topic);

int32_t kDenseDoubleRow = 5;
int32_t kSortedVectorMapRow = 6;

namespace lda {

LDAEngine::LDAEngine() {
  lda::Context& context = Context::get_instance();
  compute_ll_interval_ = context.get_int32("compute_ll_interval");
  std::string doc_file = context.get_string("doc_file");
  K_ = context.get_int32("num_topics");
  num_threads_ = context.get_int32("num_worker_threads");
  bool use_disk_stream = context.get_bool("disk_stream");
  int client_id = context.get_int32("client_id");
  if (use_disk_stream) {
    LOG_IF(INFO, client_id == 0) << "Using disk stream";
    int disk_stream_batch_size = context.get_int32("disk_stream_batch_size");
    master_loader_.reset(new DiskStreamMasterDocLoader(doc_file, num_threads_,
          K_, disk_stream_batch_size));
  } else {
    LOG_IF(INFO, client_id == 0) << "Using in memory";
    master_loader_.reset(new InMemoryMasterDocLoader(doc_file, num_threads_,
          K_));
  }
  context.set("num_vocabs", master_loader_->GetNumVocabs());
  context.set("max_vocab_id", master_loader_->GetMaxVocabID());
}

void LDAEngine::InitApp() { }

std::vector<petuum::TableConfig> LDAEngine::ConfigTables() {
  petuum::PSTableGroup::RegisterRow<
      petuum::DenseRow<double>>((petuum::RowType) kDenseDoubleRow);

  petuum::PSTableGroup::RegisterRow<
      petuum::SortedVectorMapRow<int32_t>>((petuum::RowType) kSortedVectorMapRow);

  std::vector<petuum::TableConfig> table_configs;
  lda::Context& context = lda::Context::get_instance();

  petuum::TableConfig config;

  // Summary row table (single_row).
  config.name = "summary";
  config.row_type = petuum::kDenseIntRow;
  config.num_cols = FLAGS_num_topics;
  config.num_rows = 1;
  config.staleness = context.get_int32("summary_table_staleness");
  config.num_rows_to_cache = 1;
  config.num_oplog_rows = 1;
  table_configs.push_back(config);

  // Word-topic table.
  config.name = "word_topic";
  config.row_type = (petuum::RowType) kSortedVectorMapRow;
  config.num_cols = 0;
  config.num_rows = 1;
  config.staleness = context.get_int32("word_topic_table_staleness");
  if (context.get_int32("num_clients") == 1) {
    config.num_rows_to_cache = context.get_int32("ps_row_in_memory_limit");
  }
  else {
    if (context.get_int32("word_topic_table_process_cache_capacity") == -1) {
      config.num_rows_to_cache =
        context.get_int32("max_vocab_id") + 1;
    } else {
      config.num_rows_to_cache =
        context.get_int32("word_topic_table_process_cache_capacity");
    }
  }
  config.oplog_type = petuum::RowOpLogType::kSparseRowOpLog;
  config.num_oplog_rows = config.num_rows_to_cache;
  table_configs.push_back(config);

    // Log-likelihood (llh) table. Single column; each column is a complete-llh.
  config.name = "llh";
  config.row_type = (petuum::RowType) kDenseDoubleRow;
  config.num_cols = 3;
  config.num_rows = 1;
  config.staleness = 0;
  config.num_rows_to_cache = context.get_int32("num_work_units") * context.get_int32("num_iters_per_work_unit");
  config.num_oplog_rows = context.get_int32("num_work_units") * context.get_int32("num_iters_per_work_unit");
  table_configs.push_back(config);

  return table_configs;
}

void LDAEngine::WorkerThread(int client_id, int thread_id) {

  petuum::Table<int> summary_table = GetTable<int>("summary");
  petuum::Table<int> word_topic_table = GetTable<int>("word_topic");
  petuum::Table<double> llh_table = GetTable<double>("llh");

  Context& context = Context::get_instance();
  //int client_id = context.get_int32("client_id");
  int32_t num_work_units = context.get_int32("num_work_units");
  int32_t num_clocks_per_work_unit =
    context.get_int32("num_clocks_per_work_unit");
  int32_t num_iters_per_work_unit =
    context.get_int32("num_iters_per_work_unit");
  WorkloadManager workload_mgr(
      master_loader_->GetPartitionDocLoader(thread_id),
      num_iters_per_work_unit, num_clocks_per_work_unit);

  // Tally the word-topic counts for this thread. Use batch to reduce row
  // contentions.
  petuum::UpdateBatch<int> summary_updates;
  std::unordered_map<int, petuum::UpdateBatch<int> > word_topic_updates;
  std::set<int32_t> local_vocabs;

  int num_docs = 0;
  while (!workload_mgr.IsEndOfAnIter()) {
    auto doc_ptr = workload_mgr.GetOneDoc();
    ++num_docs;
    for (LDADoc::Iterator it(doc_ptr); !it.IsEnd(); it.Next()) {
      local_vocabs.insert(it.Word());
      word_topic_updates[it.Word()].Update(it.Topic(), 1);
      summary_updates.Update(it.Topic(), 1);
    }
  }

  for (auto it = word_topic_updates.begin();
      it != word_topic_updates.end(); ++it) {
    word_topic_table.BatchInc(it->first, it->second);
  }
  summary_table.BatchInc(0, summary_updates);

  petuum::PSTableGroup::GlobalBarrier();
  {
    std::unique_lock<std::mutex> ulock(local_vocabs_mtx_);
    for(auto it = local_vocabs.begin(); it != local_vocabs.end(); ++it) {
      local_vocabs_.insert(*it);
    }
  }

  if (thread_id == 0) {
    for (auto it = local_vocabs_.begin(); it != local_vocabs_.end(); ++it) {
      word_topic_table.GetAsync(*it);
    }
    word_topic_table.WaitPendingAsyncGet();
  }

  // Sync after initialization using process_barrier_ from the parent class
  process_barrier_->wait();

  int num_clients = context.get_int32("num_clients");
  int max_vocab_id = master_loader_->GetMaxVocabID();
  // How many vocabs a thread is in charged of for computing LLH.
  int num_vocabs_per_thread = max_vocab_id / (num_clients * num_threads_);
  // Figure out the range of vocabs this thread works on.
  int vocab_id_start = (client_id * num_threads_ +
      thread_id) * num_vocabs_per_thread;
  int vocab_id_end = (client_id == (num_clients - 1)
      && thread_id == (num_threads_ - 1))
    ? max_vocab_id + 1 : vocab_id_start + num_vocabs_per_thread;

  if (thread_id == 0) {
    // LDAStats's initialization requires FLAGS_num_vocabs to be set.
    lda_stats_.reset(new LDAStats(summary_table, word_topic_table, llh_table));
    LOG(INFO) << "training starts";
  }

  FastDocSampler sampler(summary_table, word_topic_table);

  petuum::HighResolutionTimer total_timer;  // times the computation.

  for (int work_unit = 1; work_unit <= num_work_units; ++work_unit) {

    petuum::HighResolutionTimer work_unit_timer;

    // Refresh summary row cache every iteration.
    sampler.RefreshCachedSummaryRow();

    int32_t num_clocks_this_work_unit = 0;

    workload_mgr.RestartWorkUnit();

    while (!workload_mgr.IsEndOfWorkUnit()) {
      LDADoc* doc = workload_mgr.GetOneDoc();

      sampler.SampleOneDoc(doc);

      if (workload_mgr.NeedToClock()) {
        petuum::PSTableGroup::Clock();
        // This refresh is stricly needed as thread cache is invalidated by
        // Clock().
        sampler.RefreshCachedSummaryRow();
        ++num_clocks_this_work_unit;
      }
    }
    double seconds_this_work_unit = work_unit_timer.elapsed();
    LOG_IF(INFO, thread_id == 0)
    //LOG(INFO)
      << "work_unit: " << work_unit
      << "\titer: " << num_iters_per_work_unit
      << "\tclient " << client_id
      << "\tthread_id " << thread_id
      << "\ttook: " << seconds_this_work_unit << " sec"
      << "\tthroughput: "
      << workload_mgr.GetNumTokens() * num_iters_per_work_unit
      / seconds_this_work_unit
      << " token/(thread*sec)";

    // Compute LLH.
    if (compute_ll_interval_ != -1 && work_unit % compute_ll_interval_ == 0) {
      int ith_llh = work_unit / compute_ll_interval_;
      int iter = work_unit * num_iters_per_work_unit;
      ComputeLLH(ith_llh, iter, client_id, num_clients, thread_id,
          vocab_id_start, vocab_id_end, total_timer, &workload_mgr);

      // Head thread write out topic word stats (phi).
      if (FLAGS_output_topic_word && client_id == 0 && thread_id == 0) {
        SaveTopicWordDist();
      }
      if (FLAGS_output_doc_topic) {
        SaveDocTopicDist(&workload_mgr);
      }
    }
  }   // for iter.

  petuum::PSTableGroup::GlobalBarrier();
  // Head thread print out the LLH.
  if (client_id == 0 && thread_id == 0) {
    int num_llh = num_work_units / compute_ll_interval_;
    LOG(INFO) << "\n" << lda_stats_->PrintLLH(num_llh);
    SaveLLH(num_llh);
    LOG(INFO) << "Total time for " << num_work_units << " work units : "
      << total_timer.elapsed() << " sec.";
  }
}

namespace {

// 'doc_topic_vec' needs to have size == num topics.
std::vector<float> ComputeDocTopicVector(LDADoc* doc) {
  std::vector<float> doc_topic(FLAGS_num_topics);
  for (LDADoc::Iterator it(doc); !it.IsEnd(); it.Next()) {
    doc_topic[it.Topic()] += 1;
  }
  float norm = doc->GetNumTokens() + FLAGS_num_topics * FLAGS_alpha;
  for (int k = 0; k < FLAGS_num_topics; ++k) {
    doc_topic[k] = (doc_topic[k] + FLAGS_alpha) / norm;
  }
  return doc_topic;
}

}   // anonymous namespace

void LDAEngine::SaveDocTopicDist(WorkloadManager* workload_mgr) {
  petuum::HighResolutionTimer disk_output_timer;
  std::string output_file = FLAGS_output_file_prefix + ".theta." +
    std::to_string(FLAGS_client_id);
  std::ofstream out_stream(output_file);
  CHECK(out_stream) << "Failed to open output_file " << output_file;
  workload_mgr->RestartWorkUnit();
  while (!workload_mgr->IsEndOfAnIter()) {
    LDADoc* doc = workload_mgr->GetOneDoc();
    std::vector<float> doc_topic = ComputeDocTopicVector(doc);
    for (int k = 0; k < FLAGS_num_topics; ++k) {
      out_stream << doc_topic[k] << " ";
    }
    out_stream << std::endl;
  }
  LOG(INFO) << "doc topic (theta) was saved to "
    << output_file << " in " << disk_output_timer.elapsed() << " sec.";
}

void LDAEngine::SaveTopicWordDist() {
  petuum::HighResolutionTimer disk_output_timer;
  std::vector<std::vector<float> > phi = GetTopicWordDist();
  std::string output_file = FLAGS_output_file_prefix + ".phi";
  std::ofstream out_stream(output_file);
  CHECK(out_stream) << "Failed to open output_file " << output_file;
  int max_vocab_id = master_loader_->GetMaxVocabID();
  for (int k = 0; k < K_; ++k) {
    for (int w = 0; w < max_vocab_id + 1; ++w) {
      out_stream << phi[k][w] << " ";
    }
    out_stream << std::endl;
  }
  LOG(INFO) << "word topic (phi) was saved to "
    << output_file << " in " << disk_output_timer.elapsed() << " sec.";
}

std::vector<std::vector<float> > LDAEngine::GetTopicWordDist() {
  int max_vocab_id = master_loader_->GetMaxVocabID();

  // Topic word table.
  std::vector<std::vector<float> > topic_word_table;
  topic_word_table.resize(K_);
  for (auto& topic_row : topic_word_table) {
    topic_row.resize(max_vocab_id + 1);
  }

  petuum::Table<int> word_topic_table = GetTable<int>("word_topic");
  for (int word_idx = 0; word_idx < max_vocab_id + 1; ++word_idx) {
    petuum::RowAccessor word_topic_row_acc;
    word_topic_table.Get(word_idx, &word_topic_row_acc);
    const auto& word_topic_row =
      word_topic_row_acc.Get<petuum::SortedVectorMapRow<int32_t> >();
    for (petuum::SortedVectorMapRow<int>::const_iterator wt_it =
        word_topic_row.cbegin(); !wt_it.is_end(); ++wt_it) {
      int32_t topic = wt_it->first;
      int32_t count = wt_it->second;
      topic_word_table[topic][word_idx] += count;
    }
  }

  petuum::Table<int> summary_table = GetTable<int>("summary");
  petuum::RowAccessor summary_row_acc;
  summary_table.Get(0, &summary_row_acc);
  const auto& summary_row =
    summary_row_acc.Get<petuum::DenseRow<int> >();
  for (int k = 0; k < K_; ++k) {
    float norm = summary_row[k] + (max_vocab_id + 1) * FLAGS_beta;
    for (int word = 0; word < max_vocab_id + 1; ++word) {
      topic_word_table[k][word] =
        (topic_word_table[k][word] + FLAGS_beta) / norm;
    }
  }
  return topic_word_table;
}

void LDAEngine::SaveLLH(int up_to_ith_llh) {
  Context& context = Context::get_instance();
  // Save llh to disk.
  std::string output_file_prefix = context.get_string("output_file_prefix");
  if (!output_file_prefix.empty()) {
    petuum::HighResolutionTimer disk_output_timer;
    std::string output_file = output_file_prefix + ".llh";
    std::ofstream out_stream(output_file.c_str());
    CHECK(out_stream) << "Failed to open output_file " << output_file;
    out_stream << lda_stats_->PrintLLH(up_to_ith_llh);
    out_stream.close();
    LOG(INFO) << "LLH 1 ~ " << up_to_ith_llh << " is saved to "
      << output_file << " in " << disk_output_timer.elapsed() << " sec.";
  }
}

void LDAEngine::ComputeLLH(int32_t ith_llh, int32_t iter, int32_t client_id,
    int32_t num_clients, int32_t thread_id, int32_t vocab_id_start, int32_t
    vocab_id_end, const petuum::HighResolutionTimer& total_timer,
    WorkloadManager* workload_mgr) {
  Context& context = Context::get_instance();
  petuum::Table<double> llh_table = GetTable<double>("llh");
  // Every thread compute doc-topic LLH component.

  petuum::HighResolutionTimer llh_timer;
  workload_mgr->RestartWorkUnit();
  double doc_llh = 0.;
  while (!workload_mgr->IsEndOfAnIter()) {
    LDADoc* doc = workload_mgr->GetOneDoc();
    doc_llh += lda_stats_->ComputeOneDocLLH(doc);
  }
  // TODO(jinliang): could use thread inc
  llh_table.Inc(ith_llh, kColIdxLLHTableLLH, doc_llh);

  // Each client takes turn to compute the last bit of word llh and
  // print LLH from to last iteration to get correct llh (llh_table should
  // have 0 staleness.
  if (ith_llh % num_clients == client_id && thread_id == 0) {
    lda_stats_->ComputeWordLLH(ith_llh, vocab_id_start, vocab_id_end);
    lda_stats_->ComputeWordLLHSummary(ith_llh, iter);
    LOG(INFO) << "LLH compute time: " << llh_timer.elapsed() << " seconds.";
    LOG_IF(INFO, ith_llh > 1) << " LLH (work_unit "
      << (ith_llh - 1) * compute_ll_interval_
      << "): " << lda_stats_->PrintOneLLH(ith_llh - 1);

    // Save llh to disk (in case of a crash).
    SaveLLH(ith_llh - 1);
    lda_stats_->SetTime(ith_llh, total_timer.elapsed());
  } else {
    lda_stats_->ComputeWordLLH(ith_llh, vocab_id_start, vocab_id_end);
  }
}

}   // namespace lda
