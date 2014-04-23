// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.03.25

#include "lda_engine.hpp"
#include "high_resolution_timer.hpp"
#include <petuum_ps/include/petuum_ps.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <thread>
#include <vector>

// Petuum Parameters
DEFINE_string(hostfile, "", "Path to file containing server ip:port.");
DEFINE_int32(num_clients, 1, "Total number of clients");
DEFINE_int32(num_worker_threads, 1, "Number of app threads in this client");
DEFINE_int32(client_id, 0, "Client ID");

// Table Configs
DEFINE_int32(word_topic_table_process_cache_capacity, 100000, "");

// LDA Parameters
DEFINE_string(doc_file, "",
    "File containing document in LibSVM format. Each document is a line.");
DEFINE_int32(num_vocabs, -1, "Number of vocabs.");
DEFINE_double(alpha, 1, "Dirichlet prior on document-topic vectors.");
DEFINE_double(beta, 0.01, "Dirichlet prior on vocab-topic vectors.");
DEFINE_int32(num_topics, 100, "Number of topics.");
DEFINE_int32(num_iterations, 10, "Number of iterations");
DEFINE_int32(compute_ll_interval, 1,
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
DEFINE_int32(llh_table_id, 3, "ID within Petuum-PS");
DEFINE_bool(aggressive_cpu, false, "Aggressive usage of CPU");

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  // Read in data first to get # of vocabs in this partition.
  lda::LDAEngine lda_engine;
  util::HighResolutionTimer reading_timer;
  int32_t num_partition_vocabs = lda_engine.ReadData(FLAGS_doc_file);
  LOG(INFO) << "Finished reading data in " << reading_timer.elapsed()
    << " seconds.";

  petuum::TableGroupConfig table_group_config;
  // 1 server thread per client
  table_group_config.num_total_server_threads = FLAGS_num_clients;
  // 1 background thread per client
  table_group_config.num_total_bg_threads = FLAGS_num_clients;
  table_group_config.num_total_clients = FLAGS_num_clients;
  // doc-topic table, summary table, llh table.
  table_group_config.num_tables = 3;
  table_group_config.num_local_server_threads = 1;
  // + 1 for main() thread.
  table_group_config.num_local_app_threads = FLAGS_num_worker_threads + 1;
  table_group_config.num_local_bg_threads = 1;

  petuum::GetHostInfos(FLAGS_hostfile, &table_group_config.host_map);
  petuum::GetServerIDsFromHostMap(&(table_group_config.server_ids),
    table_group_config.host_map);

  table_group_config.client_id = FLAGS_client_id;
  table_group_config.consistency_model = petuum::SSPPush;
  table_group_config.aggressive_cpu = FLAGS_aggressive_cpu;

  int32_t sorted_vector_map_row_type_id = 1;
  int32_t dense_row_int_type_id = 2;
  int32_t dense_row_double_type_id = 3;
  petuum::TableGroup::RegisterRow<petuum::SortedVectorMapRow<int32_t> >
    (sorted_vector_map_row_type_id);
  petuum::TableGroup::RegisterRow<petuum::DenseRow<int32_t> >
    (dense_row_int_type_id);
  petuum::TableGroup::RegisterRow<petuum::DenseRow<double> >
    (dense_row_double_type_id);

  // Don't let main thread access table API.
  int32_t init_thread_id = petuum::TableGroup::Init(table_group_config, false);
  //LOG(INFO) << "Initialized TableGroup, init thread id = " << init_thread_id;

  // Word-topic table (not including summary row).
  petuum::ClientTableConfig wt_table_config;
  wt_table_config.table_info.table_staleness
    = FLAGS_word_topic_table_staleness;
  wt_table_config.table_info.row_type = sorted_vector_map_row_type_id;
  wt_table_config.table_info.row_capacity = 0;  // ignored.
  wt_table_config.process_cache_capacity =
    FLAGS_word_topic_table_process_cache_capacity;
  wt_table_config.thread_cache_capacity = 1;
  wt_table_config.oplog_capacity = num_partition_vocabs;
  CHECK(petuum::TableGroup::CreateTable(FLAGS_word_topic_table_id,
        wt_table_config)) << "Failed to create word-topic table";

  // Summary row table (single_row).
  petuum::ClientTableConfig summary_table_config;
  summary_table_config.table_info.table_staleness
    = FLAGS_summary_table_staleness;
  summary_table_config.table_info.row_type = dense_row_int_type_id;
  summary_table_config.table_info.row_capacity = FLAGS_num_topics;
  summary_table_config.process_cache_capacity = 1;
  summary_table_config.thread_cache_capacity = 1;
  summary_table_config.oplog_capacity = 1;
  CHECK(petuum::TableGroup::CreateTable(FLAGS_summary_table_id,
        summary_table_config)) << "Failed to create summary table";

  // Log-likelihood (llh) table. Single column; each column is a complete-llh.
  petuum::ClientTableConfig llh_table_config;
  // Can have infinite staleness as it is write only (no read till end of
  // program).
  llh_table_config.table_info.table_staleness = FLAGS_summary_table_staleness;
  llh_table_config.table_info.row_type = dense_row_double_type_id;
  llh_table_config.table_info.row_capacity = 2;   // two columns: "iter-# llh".
  llh_table_config.process_cache_capacity = FLAGS_num_iterations;
  llh_table_config.thread_cache_capacity = 1;
  llh_table_config.oplog_capacity = FLAGS_num_iterations;
  CHECK(petuum::TableGroup::CreateTable(FLAGS_llh_table_id,
        llh_table_config)) << "Failed to create summary table";

  petuum::TableGroup::CreateTableDone();

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
  petuum::TableGroup::ShutDown();
  LOG(INFO) << "LDA shut down!";
  return 0;
}
