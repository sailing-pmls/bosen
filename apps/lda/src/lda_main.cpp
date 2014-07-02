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
// Date: 2014.03.25

#include "lda_engine.hpp"
#include "context.hpp"
#include <petuum_ps_common/include/petuum_ps.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <thread>
#include <vector>

// Petuum Parameters
DEFINE_string(hostfile, "", "Path to file containing server ip:port.");
DEFINE_int32(num_clients, 1, "Total number of clients");
DEFINE_int32(num_worker_threads, 1, "Number of app threads in this client");
DEFINE_int32(client_id, 0, "Client ID");
DEFINE_int32(word_topic_table_process_cache_capacity, -1,
    "Word topic table process cache capacity. -1 uses FLAGS_num_vocabs.");
DEFINE_int32(ps_row_in_memory_limit, 50000, "Single-machine version only: "
    "max # rows word-topic table that can be held in memory");

// LDA Parameters
DEFINE_string(doc_file, "",
    "File containing document in LibSVM format. Each document is a line.");
DEFINE_double(alpha, 1, "Dirichlet prior on document-topic vectors.");
DEFINE_double(beta, 0.1, "Dirichlet prior on vocab-topic vectors.");
DEFINE_int32(num_topics, 100, "Number of topics.");
DEFINE_int32(num_work_units, 1, "Number of work units");
DEFINE_int32(compute_ll_interval, 1,
    "Copmute log likelihood over local dataset on every compute_ll_interval "
    "work units");

// Misc
DEFINE_int32(word_topic_table_staleness, 0, "staleness for word-topic table.");
DEFINE_int32(summary_table_staleness, 0,
    "staleness for table containing summary row.");
DEFINE_string(output_file_prefix, "", "LDA results.");
DEFINE_bool(disk_stream, false,
    "Turn it on when memory isn't enough for the dataset.");
DEFINE_int32(disk_stream_batch_size, 1000,
    "Each worker thread gets disk_stream_batch_size docs at a time.");

// No need to change the following.
DEFINE_int32(word_topic_table_id, 1, "ID within Petuum-PS");
DEFINE_int32(summary_table_id, 2, "ID within Petuum-PS");
DEFINE_int32(llh_table_id, 3, "ID within Petuum-PS");
DEFINE_bool(aggressive_cpu, false, "Aggressive usage of CPU");
DEFINE_int32(num_vocabs, -1, "Number of vocabs. This is read from meta-data file.");
DEFINE_int32(max_vocab_id, -1, "Maximum word index, which could be different "
    "from num_vocabs if there are unused vocab indices. "
    "This is read from meta-data file.");

DEFINE_string(stats_path, "", "Statistics output file");

DEFINE_string(consistency_model, "SSPPush", "SSP or SSPPush or ...");

DEFINE_int32(num_bg_threads, 1, "number of background threads");
DEFINE_int32(num_server_threads, 1, "number of server threads");

DEFINE_int32(num_iters_per_work_unit, 1, "number of iterations per work unit");
DEFINE_int32(num_clocks_per_work_unit, 1, "number of clocks per work unit");

int32_t kSortedVectorMapRowTypeID = 1;
int32_t kDenseRowIntTypeID = 2;
int32_t kDenseRowDoubleTypeID = 3;

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  // Read in data first to get # of vocabs in this partition.
  petuum::TableGroupConfig table_group_config;
  // 1 server thread per client
  table_group_config.num_total_server_threads = FLAGS_num_clients*FLAGS_num_server_threads;
  // 1 background thread per client
  table_group_config.num_total_bg_threads = FLAGS_num_clients*FLAGS_num_bg_threads;
  table_group_config.num_total_clients = FLAGS_num_clients;
  // doc-topic table, summary table, llh table.
  table_group_config.num_tables = 3;
  table_group_config.num_local_server_threads = FLAGS_num_server_threads;
  // + 1 for main() thread.
  table_group_config.num_local_app_threads = FLAGS_num_worker_threads + 1;
  table_group_config.num_local_bg_threads = FLAGS_num_bg_threads;

#ifdef PETUUM_SINGLE_NODE
  table_group_config.ooc_path_prefix = FLAGS_output_file_prefix
    + ".lda_sn.localooc";
  table_group_config.consistency_model = petuum::LocalOOC;
#else
  table_group_config.client_id = FLAGS_client_id;
  petuum::GetHostInfos(FLAGS_hostfile, &table_group_config.host_map);
  petuum::GetServerIDsFromHostMap(&(table_group_config.server_ids),
      table_group_config.host_map);
  if (std::string("SSP").compare(FLAGS_consistency_model) == 0) {
    table_group_config.consistency_model = petuum::SSP;
  } else if (std::string("SSPPush").compare(FLAGS_consistency_model) == 0) {
    table_group_config.consistency_model = petuum::SSPPush;
  } else {
    LOG(FATAL) << "Unkown consistency model: " << FLAGS_consistency_model;
  }
#endif

  table_group_config.aggressive_cpu = FLAGS_aggressive_cpu;
  table_group_config.stats_path = FLAGS_stats_path;

  petuum::PSTableGroup::RegisterRow<petuum::SortedVectorMapRow<int32_t> >
    (kSortedVectorMapRowTypeID);
  petuum::PSTableGroup::RegisterRow<petuum::DenseRow<int32_t> >
    (kDenseRowIntTypeID);
  petuum::PSTableGroup::RegisterRow<petuum::DenseRow<double> >
    (kDenseRowDoubleTypeID);

  // Don't let main thread access table API.
  petuum::PSTableGroup::Init(table_group_config, false);

  STATS_SET_APP_DEFINED_ACCUM_SEC_NAME("sole_compute_sec");
  STATS_SET_APP_DEFINED_VEC_NAME("work_unit_sec");
  STATS_SET_APP_DEFINED_ACCUM_VAL_NAME("nonzero_entries");

  STATS_APP_LOAD_DATA_BEGIN();
  // LDAEngine constructor sets max_vocab_id and num_vocabs in Context.
  lda::LDAEngine lda_engine;
  LOG(INFO) << "Read data done!";
  STATS_APP_LOAD_DATA_END();

  // Word-topic table (not including summary row).
  petuum::ClientTableConfig wt_table_config;
  wt_table_config.table_info.table_staleness
    = FLAGS_word_topic_table_staleness;
  wt_table_config.table_info.row_type = kSortedVectorMapRowTypeID;
  wt_table_config.table_info.row_capacity = 0;  // ignored.
#ifdef PETUUM_SINGLE_NODE
  wt_table_config.process_cache_capacity = FLAGS_ps_row_in_memory_limit;
#else
  lda::Context& context = lda::Context::get_instance();
  if (FLAGS_word_topic_table_process_cache_capacity == -1) {
    wt_table_config.process_cache_capacity = context.get_int32("num_vocabs");
  } else {
    wt_table_config.process_cache_capacity =
      FLAGS_word_topic_table_process_cache_capacity;
  }
#endif
  wt_table_config.thread_cache_capacity = 1;
  wt_table_config.oplog_capacity = wt_table_config.process_cache_capacity;
  CHECK(petuum::PSTableGroup::CreateTable(FLAGS_word_topic_table_id,
        wt_table_config)) << "Failed to create word-topic table";

  // Summary row table (single_row).
  petuum::ClientTableConfig summary_table_config;
  summary_table_config.table_info.table_staleness
    = FLAGS_summary_table_staleness;
  summary_table_config.table_info.row_type = kDenseRowIntTypeID;
  summary_table_config.table_info.row_capacity = FLAGS_num_topics;
  summary_table_config.process_cache_capacity = 1;
  summary_table_config.thread_cache_capacity = 1;
  summary_table_config.oplog_capacity = 1;
  CHECK(petuum::PSTableGroup::CreateTable(FLAGS_summary_table_id,
        summary_table_config)) << "Failed to create summary table";

  // Log-likelihood (llh) table. Single column; each column is a complete-llh.
  petuum::ClientTableConfig llh_table_config;
  // Use no staleness to print llh of previous iteration. (Note that computing
  // llh already affects the execution, so staleness doesn't change much.)
  llh_table_config.table_info.table_staleness = 0;
  llh_table_config.table_info.row_type = kDenseRowDoubleTypeID;
  llh_table_config.table_info.row_capacity = 3;   // 3 columns: "iter-# llh time".
  llh_table_config.process_cache_capacity =
    FLAGS_num_work_units * FLAGS_num_iters_per_work_unit;
  llh_table_config.thread_cache_capacity = 1;
  llh_table_config.oplog_capacity =
    FLAGS_num_work_units * FLAGS_num_iters_per_work_unit;
  CHECK(petuum::PSTableGroup::CreateTable(FLAGS_llh_table_id,
        llh_table_config)) << "Failed to create summary table";

  petuum::PSTableGroup::CreateTableDone();

  // Start LDA
  LOG(INFO) << "Starting LDA with " << FLAGS_num_worker_threads << " threads "
    << "on client " << FLAGS_client_id;

  std::vector<std::thread> threads(FLAGS_num_worker_threads);
  for (auto& thr : threads) {
    thr = std::thread(&lda::LDAEngine::Start, std::ref(lda_engine));
  }
  for (auto& thr : threads) {
    thr.join();
  }

  LOG(INFO) << "LDA finished!";
  petuum::PSTableGroup::ShutDown();
  LOG(INFO) << "LDA shut down!";
  return 0;
}
