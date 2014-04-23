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

// Author: Xun Zheng (xunzheng@cs.cmu.edu), Dai Wei (wdai@cs.cmu.edu)
// Date: 2013.11.21

#include "lda_engine.hpp"
#include <petuum_ps/include/petuum_ps.hpp>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <vector>

// Petuum Parameters
DEFINE_string(hostfile, "", "Path to file containing server ip:port.");
DEFINE_int32(num_clients, 1, "Total number of clients");
DEFINE_int32(num_worker_threads, 2, "Number of app threads in this client");
DEFINE_int32(client_id, 0, "Client ID");

// Table configs.
DEFINE_int32(doc_topic_table_process_cache_capacity, 100000, "");
DEFINE_int32(doc_topic_table_staleness, 0, "staleness for word-topic table.");
DEFINE_int32(summary_table_staleness, 0,
    "staleness for table containing summary row.");

// LDA Inputs:
DEFINE_string(data_file, "",
    "File containing document in LibSVM format. Each document is a line.");
DEFINE_string(vocab_file, "",
    "File containing a list of vocab, each on a line.");
DEFINE_double(alpha, 1, "Dirichlet prior on document-topic vectors.");
DEFINE_double(beta, 0.01, "Dirichlet prior on vocab-topic vectors.");
DEFINE_int32(num_topics, 100, "Number of topics.");
DEFINE_int32(num_iterations, 100, "Number of iterations");
DEFINE_int32(compute_ll_interval, 10,
    "Copmute log likelihood over local dataset on every N iterations");
DEFINE_bool(head_client, false, "If it is the contributor of doc likelihood");
DEFINE_string(output_prefix, "", "Prefix for output files. Use abs path.");

// Misc
DEFINE_int32(num_top_words, 10, "Num of top words to display");

// No need to change the following.
DEFINE_int32(doc_topic_table_id, 1, "ID within Petuum-PS");
DEFINE_int32(summary_table_id, 2, "ID within Petuum-PS");

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  LOG(INFO) << "LDA started on client " << FLAGS_client_id
    << " num_worker_threads: " << FLAGS_num_worker_threads;

  LOG_IF(INFO, FLAGS_head_client) << "client " << FLAGS_client_id
      << " is the ONE AND ONLY head client";

  petuum::TableGroupConfig table_group_config;
  // 1 server thread per client
  table_group_config.num_total_server_threads = FLAGS_num_clients;
  // 1 background thread per client
  table_group_config.num_total_bg_threads = FLAGS_num_clients;
  table_group_config.num_total_clients = FLAGS_num_clients;
  table_group_config.num_tables = 2;  // doc-topic table, summary table
  table_group_config.num_local_server_threads = 1;
  // +1 for the main() thread.
  table_group_config.num_local_app_threads = FLAGS_num_worker_threads + 1;
  table_group_config.num_local_bg_threads = 1;

  petuum::GetHostInfos(FLAGS_hostfile, &table_group_config.host_map);

  petuum::GetServerIDsFromHostMap(&(table_group_config.server_ids),
    table_group_config.host_map);

  table_group_config.client_id = FLAGS_client_id;
  const uint32_t threadspace_per_process = 1000;
  table_group_config.local_id_min = FLAGS_client_id * threadspace_per_process;
  table_group_config.local_id_max =
    (FLAGS_client_id+1) * threadspace_per_process - 1;
  table_group_config.consistency_model = petuum::SSP;

  int32_t dense_row_type_id = 1;
  petuum::TableGroup::RegisterRow<petuum::DenseRow<int32_t> >
    (dense_row_type_id);

  // Don't let main thread access table API.
  int32_t init_thread_id = petuum::TableGroup::Init(table_group_config, false);

  LOG(INFO) << "Initialized TableGroup, init thread id = " << init_thread_id;

  // Summary row table (single_row).
  petuum::ClientTableConfig summary_table_config;
  summary_table_config.table_info.table_staleness
    = FLAGS_summary_table_staleness;
  summary_table_config.table_info.row_type = dense_row_type_id;
  summary_table_config.table_info.row_capacity = FLAGS_num_topics;
  summary_table_config.process_cache_capacity = 1;
  summary_table_config.thread_cache_capacity = 1;
  summary_table_config.oplog_capacity = 100;
  CHECK(petuum::TableGroup::CreateTable(FLAGS_summary_table_id,
        summary_table_config)) << "Failed to create summary table";

  // Construct theta (document-topic) table: [N rows x K columns].
  petuum::ClientTableConfig doc_topic_table_config;
  doc_topic_table_config.table_info.table_staleness
    = FLAGS_doc_topic_table_staleness;
  doc_topic_table_config.table_info.row_type = dense_row_type_id;
  doc_topic_table_config.table_info.row_capacity = FLAGS_num_topics;
  doc_topic_table_config.process_cache_capacity =
    FLAGS_doc_topic_table_process_cache_capacity;
  doc_topic_table_config.thread_cache_capacity = 100;
  doc_topic_table_config.oplog_capacity = 1000;
  CHECK(petuum::TableGroup::CreateTable(FLAGS_doc_topic_table_id,
        doc_topic_table_config)) << "Failed to create doc-topic table";

  petuum::TableGroup::CreateTableDone();

  LOG(INFO) << "All tables are successfully created.";

  lda::LDAEngine lda_sampler;

  // One thread reads the data.
  //lda_sampler.ReadData();

  LOG(INFO) << "Done reading dataset.";

  // Create FLAGS_num_threads to run lda_sampler.RunSampler()
  boost::thread_group worker_threads;
  for (int i = 0; i < FLAGS_num_worker_threads; ++i) {
    worker_threads.create_thread(
        boost::bind(&lda::LDAEngine::Start, boost::ref(lda_sampler)));
  }
  // Needed when the main thread is accessing table API.
  petuum::TableGroup::WaitThreadRegister();

  worker_threads.join_all();

  LOG(INFO) << "Done! Shutting down.";
  petuum::TableGroup::ShutDown();
  return 0;
}
