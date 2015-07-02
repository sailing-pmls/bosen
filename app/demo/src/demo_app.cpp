#include <petuum_ps_common/include/petuum_ps.hpp>
#include <io/general_fstream.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <thread>
#include <vector>

DEFINE_string(hostfile, "", "Path to file containing server ip:port.");
DEFINE_int32(num_clients, 1, "# of nodes");
DEFINE_int32(client_id, 0, "Client ID");
DEFINE_int32(num_threads, 1, "# of worker threads.");
DEFINE_int32(num_iterations, 10, "# of iterations.");
DEFINE_int32(staleness, 0, "Staleness.");

DEFINE_string(input_file, "", "local file or hdfs://host:port/file");
DEFINE_string(output_file, "", "local file or hdfs://host:port/file");

// DemoAPP demonstrates the concept of stale synchronous parallel (SSP). It
// uses 1 table with 1 row, with num_workers columns (FLAGS_num_clients *
// FLAGS_num_threads), each entry is the clock of one worker as viewed by each
// worker. SSP ensures that the clock view will be at most 'staleness' clocks
// apart, which we verify here.

namespace {

const int kDenseRowType = 0;
const int kTableID = 0;

void DemoAPPWorker(int worker_rank) {
  petuum::PSTableGroup::RegisterThread();
  petuum::Table<int> clock_table =
    petuum::PSTableGroup::GetTableOrDie<int>(kTableID);
  int num_workers = FLAGS_num_clients * FLAGS_num_threads;
  std::vector<int> row_cache;
  for (int clock = 0; clock < FLAGS_num_iterations; ++clock) {
    petuum::RowAccessor row_acc;
    clock_table.Get(0, &row_acc);
    const auto& row = row_acc.Get<petuum::DenseRow<int>>();
    // Cache the row to a local vector to avoid acquiring lock when accessing
    // each entry.
    row.CopyToVector(&row_cache);
    for (int w = 0; w < num_workers; ++w) {
      CHECK_GE(row_cache[w], clock - FLAGS_staleness) << "I'm worker "
        << worker_rank << " seeing w: " << w;
    }
    // Update clock entry associated with this worker.
    clock_table.Inc(0, worker_rank, 1);
    petuum::PSTableGroup::Clock();
  }

  // After global barrier, all clock view should be up-to-date.
  petuum::PSTableGroup::GlobalBarrier();
  petuum::RowAccessor row_acc;
  clock_table.Get(0, &row_acc);
  const auto& row = row_acc.Get<petuum::DenseRow<int>>();
  row.CopyToVector(&row_cache);
  for (int w = 0; w < num_workers; ++w) {
    CHECK_EQ(row_cache[w], FLAGS_num_iterations);
  }
  LOG(INFO) << "Worker " << worker_rank << " verified all clock reads.";
  petuum::PSTableGroup::DeregisterThread();
}

} // anonymous namespace

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  petuum::TableGroupConfig table_group_config;
  table_group_config.num_comm_channels_per_client = 1;
  table_group_config.num_total_clients = FLAGS_num_clients;
  table_group_config.num_tables = 1;    // just the clock table
  // + 1 for main() thread.
  table_group_config.num_local_app_threads = FLAGS_num_threads + 1;
  table_group_config.client_id = FLAGS_client_id;
  table_group_config.consistency_model = petuum::SSPPush;

  petuum::GetHostInfos(FLAGS_hostfile, &table_group_config.host_map);

  petuum::PSTableGroup::RegisterRow<petuum::DenseRow<int>>(kDenseRowType);

  petuum::PSTableGroup::Init(table_group_config, false);

  // Creating test table.
  petuum::ClientTableConfig table_config;
  table_config.table_info.row_type = kDenseRowType;
  table_config.table_info.table_staleness = FLAGS_staleness;
  table_config.table_info.row_capacity = FLAGS_num_clients * FLAGS_num_threads;
  table_config.process_cache_capacity = 1;    // only 1 row.
  table_config.table_info.row_oplog_type = petuum::RowOpLogType::kDenseRowOpLog;
  // no need to squeeze out 0's
  table_config.table_info.oplog_dense_serialized = false;
  table_config.table_info.dense_row_oplog_capacity =
    table_config.table_info.row_capacity;
  table_config.oplog_capacity = table_config.process_cache_capacity;
  CHECK(petuum::PSTableGroup::CreateTable(kTableID, table_config));

  petuum::PSTableGroup::CreateTableDone();

  LOG(INFO) << "Starting DemoAPP with " << FLAGS_num_threads << " threads "
    << "on client " << FLAGS_client_id;
  // load data
  std::vector<std::string> vec;
  petuum::io::ifstream input(FLAGS_input_file);
  std::string line;
  LOG(INFO) << "Reading " << FLAGS_input_file << "...";
  while (!input.eof()) {
    std::getline(input, line);
    LOG(INFO) << line;
  }
  std::vector<std::thread> threads(FLAGS_num_threads);
  int worker_rank = FLAGS_client_id * FLAGS_num_threads;
  for (auto& thr : threads) {
    thr = std::thread(DemoAPPWorker, worker_rank++);
  }
  for (auto& thr : threads) {
    thr.join();
  }

  petuum::PSTableGroup::ShutDown();
  // output results
  if (FLAGS_client_id == 0) {
    // Let one client write to FS.
    petuum::io::ofstream output(FLAGS_output_file);
    output << "program output" << std::endl;
  }
  LOG(INFO) << "DemoAPP Finished!";
  return 0;
}
