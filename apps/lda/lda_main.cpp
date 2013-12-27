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

#include "lda_sampler.hpp"
#include <petuum_ps/include/petuum_ps.hpp>
#include <petuum_ps/util/utils.hpp>
#include <zmq.hpp>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <vector>

// Petuum Inputs:
DEFINE_string(server_file, "",
    "Path to file containing server ip:port.");
DEFINE_string(config_file, "",
    "Path to .cfg file containing client configs.");
DEFINE_int32(num_threads, 1, "Number of worker threads");
DEFINE_int32(client_id, 10000, "ID of a client process");

// LDA Inputs:
DEFINE_string(data_file, "",
    "File containing document in LibSVM format. Each document is a line.");
DEFINE_string(vocab_file, "",
    "File containing a list of vocab, each on a line.");
DEFINE_double(alpha, 1, "Dirichlet prior on document-topic vectors.");
DEFINE_double(beta, 0.01, "Dirichlet prior on vocab-topic vectors.");
DEFINE_int32(num_topics, 100, "Number of topics.");
DEFINE_int32(num_iterations, 100, "Number of iterations");
DEFINE_int32(num_words_per_thread_per_iteration, 1000,
    "Number of words (vocabs) to sample per thread per iteration.");
DEFINE_int32(compute_ll_interval, 10,
    "Copmute log likelihood over local dataset on every N iterations");
DEFINE_bool(head_client, false, "If it is the contributor of doc likelihood");

// Misc
DEFINE_int32(num_top_words, 10, "Num of top words to display");


int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  LOG(INFO) << "LDA started on client " << FLAGS_client_id
    << " server_file: " << FLAGS_server_file
    << " num_threads: " << FLAGS_num_threads;

  LOG_IF(INFO, FLAGS_head_client) << "client " << FLAGS_client_id
      << " is the ONE AND ONLY head client";

  // Initialize TableGroup.
  zmq::context_t *zmq_ctx = new zmq::context_t(1);
  std::vector<petuum::ServerInfo> server_info =
    petuum::GetServerInfos(FLAGS_server_file);
  petuum::TableGroupConfig table_group_config(FLAGS_client_id,
      FLAGS_num_threads, server_info, zmq_ctx);
  petuum::TableGroup<int32_t>& table_group =
    petuum::TableGroup<int32_t>::CreateTableGroup(
        table_group_config);

  // Register the main thread.
  CHECK_EQ(0, table_group.RegisterThread()) << "Failed to register thread.";

  // Read table configs.
  std::map<int, petuum::TableConfig> table_configs_map =
    petuum::ReadTableConfigs(FLAGS_config_file);

  // Construct table which contains only one row: the summary row (row 0)
  // which has K columns.
  int32_t table_id = 1;
  LOG(INFO) << "Creating table " << table_id;
  LOG_IF(FATAL, table_configs_map.count(table_id) == 0)
    << "Table (table_id = " << table_id << ") is not in the config file "
    << FLAGS_config_file;
  petuum::DenseTable<int>& summary_row =
    table_group.CreateDenseTable(table_id,
        table_configs_map[table_id]);

  // Construct theta (document-topic) table: [N rows x K columns].
  table_id = 2;
  LOG(INFO) << "Creating table " << table_id;
  petuum::DenseTable<int>& doc_topic_table = table_group.CreateDenseTable(table_id,
        table_configs_map[table_id]);

  //table_id = 3;
  //LOG(INFO) << "Creating table " << table_id;
  //petuum::DenseTable<int>& likelihood_table = table_group.CreateDenseTable(table_id,
  //      table_configs_map[table_id]);

  LOG(INFO) << "All tables are successfully created.";

  lda::LDASampler lda_sampler(&table_group,
                              &summary_row,
                              &doc_topic_table);
                              //&likelihood_table);

  // One thread reads the data.
  lda_sampler.ReadData();

  LOG(INFO) << "Done reading dataset.";

  // Create FLAGS_num_threads to run lda_sampler.RunSampler()
  boost::thread_group worker_threads;
  for (int i = 0; i < FLAGS_num_threads; ++i) {
    worker_threads.create_thread(
        boost::bind(&lda::LDASampler::RunSampler, boost::ref(lda_sampler)));
  }
  worker_threads.join_all();

  LOG(INFO) << "Done! Shutting down table_group.";

  // Finish and exit
  table_group.DeregisterThread();
  table_group.ShutDown();
  delete zmq_ctx;

  return 0;
}
