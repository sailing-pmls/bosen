
#include "high_resolution_timer.hpp"
#include <petuum_ps/include/petuum_ps.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <thread>

// Petuum Parameters
DEFINE_string(hostfile, "", "Path to file containing server ip:port.");
DEFINE_int32(num_clients, 1, "Total number of clients");
DEFINE_int32(num_worker_threads, 2, "Number of app threads in this client");
DEFINE_int32(client_id, 0, "Client ID");
DEFINE_int32(num_pre_clock, 0, "number of clocks before first Get.");
DEFINE_int32(num_iterations, 200, "number of iterations.");

const int kTableID = 1;
const int kNumColumns = 1000;

void OneRowTest(int thread_id) {
  petuum::TableGroup::RegisterThread();

  petuum::Table<int> one_row_table =
    petuum::TableGroup::GetTableOrDie<int>(kTableID);

  petuum::TableGroup::GlobalBarrier();

  util::HighResolutionTimer timer;
  for (int iter = 0; iter < FLAGS_num_iterations; ++iter) {
    petuum::RowAccessor row_acc;
    one_row_table.Get(0, &row_acc);
    const auto& one_row = row_acc.Get<petuum::DenseRow<int> >();
    // Do something with one_row...
    for (int i = 0; i < kNumColumns; ++i) {
      one_row_table.Inc(0, i, 1);
    }
    petuum::TableGroup::Clock();
  }
  if (thread_id == 0) {
    LOG(INFO) << "sec per iteration: " << ": "
      << timer.elapsed() / FLAGS_num_iterations
      << " seconds.";
  }
  petuum::TableGroup::DeregisterThread();
}

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  petuum::TableGroupConfig table_group_config;
  // 1 server thread per client
  table_group_config.num_total_server_threads = FLAGS_num_clients;
  // 1 background thread per client
  table_group_config.num_total_bg_threads = FLAGS_num_clients;
  table_group_config.num_total_clients = FLAGS_num_clients;
  // Single table.
  table_group_config.num_tables = 1;
  table_group_config.num_local_server_threads = 1;
  // + 1 for main() thread.
  table_group_config.num_local_app_threads = FLAGS_num_worker_threads + 1;
  table_group_config.num_local_bg_threads = 1;

  petuum::GetHostInfos(FLAGS_hostfile, &table_group_config.host_map);

  petuum::GetServerIDsFromHostMap(&(table_group_config.server_ids),
    table_group_config.host_map);

  table_group_config.client_id = FLAGS_client_id;
  table_group_config.consistency_model = petuum::SSPPush;

  int32_t dense_row_int_type_id = 2;
  petuum::TableGroup::RegisterRow<petuum::DenseRow<int32_t> >
    (dense_row_int_type_id);

  // Let main thread access table API.
  int32_t init_thread_id = petuum::TableGroup::Init(table_group_config, false);
  LOG(INFO) << "Initialized TableGroup, init thread id = " << init_thread_id;

  // one row table (single_row).
  petuum::ClientTableConfig one_row_table_config;
  one_row_table_config.table_info.table_staleness = 0;
  one_row_table_config.table_info.row_type = dense_row_int_type_id;
  one_row_table_config.table_info.row_capacity = kNumColumns;
  one_row_table_config.process_cache_capacity = 1;
  one_row_table_config.thread_cache_capacity = 1;
  one_row_table_config.oplog_capacity = 1;
  CHECK(petuum::TableGroup::CreateTable(kTableID,
        one_row_table_config)) << "Failed to create summary table";

  petuum::TableGroup::CreateTableDone();

  LOG(INFO) << "Start OneRow.";
  std::thread threads[FLAGS_num_worker_threads];
  int thread_id = 0;
  for (auto& thr : threads) {
    thr = std::thread(OneRowTest, thread_id++);
  }
  for (auto& thr : threads) {
    thr.join();
  }

  LOG(INFO) << "OneRow finished!";
  petuum::TableGroup::ShutDown();
  LOG(INFO) << "OneRow shut down!";
  return 0;
}
