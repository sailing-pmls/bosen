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
#include <petuum_ps/include/petuum_ps.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include <boost/ref.hpp>

// Petuum Parameters
DEFINE_string(hostfile, "", "Path to file containing server ip:port.");
DEFINE_int32(num_total_server_threads, 1, "Total number of server threads");
DEFINE_int32(num_total_bg_threads, 1, "Total number of background threads");
DEFINE_int32(num_total_clients, 1, "Total number of clients");
DEFINE_int32(num_server_threads, 2, "Number of server threads in this client");
DEFINE_int32(num_app_threads, 2, "Number of app threads in this client");
DEFINE_int32(num_bg_threads, 1, "Number of bg threads on this client");
DEFINE_int32(local_id_min, 0, "Minimum local thread id");
DEFINE_int32(local_id_max, 0, "Maximum local thread id");
DEFINE_int32(client_id, 0, "Client ID");

// Table Configs
DEFINE_int32(word_topic_table_process_cache_capacity, 100000, "");

// LDA Parameters
DEFINE_string(doc_file, "",
    "File containing document in LibSVM format. Each document is a line.");
DEFINE_int32(num_vocabs, -1, "Number of vocabs.");
//DEFINE_string(vocab_file, "",
//    "File containing a list of vocab, each on a line.");
DEFINE_double(alpha, 0.01, "Dirichlet prior on document-topic vectors.");
DEFINE_double(beta, 0.01, "Dirichlet prior on vocab-topic vectors.");
DEFINE_int32(num_topics, 100, "Number of topics.");
DEFINE_int32(num_iterations, 100, "Number of iterations");
DEFINE_int32(compute_ll_interval, 10,
    "Copmute log likelihood over local dataset on every N iterations");
//DEFINE_bool(head_client, false, "If it is the contributor of doc likelihood");
//DEFINE_string(output_prefix, "", "Prefix for output files. Use abs path.");

// Misc
DEFINE_int32(word_topic_table_staleness, 0, "staleness for word-topic table.");
DEFINE_int32(summary_table_staleness, 0,
    "staleness for table containing summary row.");

// No need to change the following.
DEFINE_int32(word_topic_table_id, 1, "ID within Petuum-PS");
DEFINE_int32(summary_table_id, 2, "ID within Petuum-PS");


int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  int num_tables = 2;

  petuum::TableGroupConfig table_group_config;
  table_group_config.num_total_server_threads = FLAGS_num_total_server_threads;
  table_group_config.num_total_bg_threads = FLAGS_num_total_bg_threads;
  table_group_config.num_total_clients = FLAGS_num_total_clients;
  table_group_config.num_tables = num_tables;
  table_group_config.num_local_server_threads = FLAGS_num_server_threads;
  table_group_config.num_local_app_threads = FLAGS_num_app_threads;
  table_group_config.num_local_bg_threads = FLAGS_num_bg_threads;

  petuum::GetHostInfos(FLAGS_hostfile, &table_group_config.host_map);

  petuum::GetServerIDsFromHostMap(&(table_group_config.server_ids),
    table_group_config.host_map);

  table_group_config.client_id = FLAGS_client_id;
  table_group_config.local_id_min = FLAGS_local_id_min;
  table_group_config.local_id_max = FLAGS_local_id_max;
  table_group_config.consistency_model = petuum::SSP;

  int32_t sorted_vector_map_row_type_id = 0;
  int32_t dense_row_type_id = 1;
  petuum::TableGroup::RegisterRow<petuum::SortedVectorMapRow<int32_t> >
    (sorted_vector_map_row_type_id);
  petuum::TableGroup::RegisterRow<petuum::DenseRow<int32_t> >
    (dense_row_type_id);

  // Let main thread access table API.
  int32_t init_thread_id = petuum::TableGroup::Init(table_group_config, true);

  LOG(INFO) << "Initialized TableGroup, init thread id = " << init_thread_id;

  // Word-topic table (not including summary row).
  petuum::ClientTableConfig wt_table_config;
  wt_table_config.table_info.table_staleness
    = FLAGS_word_topic_table_staleness;
  wt_table_config.table_info.row_type = sorted_vector_map_row_type_id;
  wt_table_config.table_info.row_capacity = 0;  // ignored.
  wt_table_config.process_cache_capacity =
    FLAGS_word_topic_table_process_cache_capacity;
  wt_table_config.thread_cache_capacity = 0;
  wt_table_config.oplog_capacity = 10000;
  CHECK(petuum::TableGroup::CreateTable(FLAGS_word_topic_table_id,
        wt_table_config)) << "Failed to create word-topic table";

  // Summary row table (single_row).
  petuum::ClientTableConfig summary_table_config;
  summary_table_config.table_info.table_staleness
    = FLAGS_summary_table_staleness;
  summary_table_config.table_info.row_type = dense_row_type_id;
  summary_table_config.table_info.row_capacity = FLAGS_num_topics;
  summary_table_config.process_cache_capacity = 1;
  summary_table_config.thread_cache_capacity = 1;
  summary_table_config.oplog_capacity = 10000;
  CHECK(petuum::TableGroup::CreateTable(FLAGS_summary_table_id,
        summary_table_config)) << "Failed to create summary table";

  petuum::TableGroup::CreateTableDone();

  // Start LDA
  LOG(INFO) << "Starting LDA";
  lda::LDAEngine lda_engine;
  lda_engine.ReadData(FLAGS_doc_file);
  LOG(INFO) << "Done reading data";

  // Create FLAGS_num_app_threads to run lda_engine.Start()
  boost::thread_group worker_threads;
  for (int i = 0; i < FLAGS_num_app_threads - 1; ++i) {
    // false as it is not an init thread.
    worker_threads.create_thread(
        boost::bind(&lda::LDAEngine::Start, boost::ref(lda_engine), false));
  }
  // Needed when the main thread is accessing table API.
  petuum::TableGroup::WaitThreadRegister();

  // true for init thread. Init thread doesn't need to register with
  // TableGroup.
  lda_engine.Start(true);

  // Start it on main thread.
  worker_threads.join_all();

  LOG(INFO) << "LDA finished!";
  petuum::TableGroup::ShutDown();
  LOG(INFO) << "LDA shut down!";
  return 0;
}
