// Author: Binghong Chen
// Date: 2016.06.30

#include <petuum_ps_common/include/petuum_ps.hpp>
#include <petuum_ps_common/include/system_gflags.hpp>
//#include <petuum_ps_common/include/configs.hpp>
//#include <petuum_ps_common/util/utils.hpp>
#include <thread>
#include <map>
#include <vector>
#include <cstdint>
#include <glog/logging.h>
#include <gflags/gflags.h>

DECLARE_string(hostfile);
DECLARE_int32(client_id);

//DEFINE_string(hostfile, "", "path to Petuum PS server configuration file");
//DEFINE_int32(client_id, 0, "This client's ID");

namespace petuum {

enum RowType {
  kUndefinedRow = 0
  , kDenseFloatRow = 1
  , kDenseIntRow = 2
  , kSparseFloatRow = 3
  , kSparseIntRow = 4
};

struct TableConfig {
  // Required config for table.
  std::string name;   // name is used to fetch the table
  RowType row_type{kUndefinedRow};
  int64_t num_cols{-1};
  int64_t num_rows{-1};
  int staleness{0};

  // Optional configs
  // Number of rows to cache in process cache. Default -1 means all rows.
  int64_t num_rows_to_cache{-1};
  // Estimated upper bound # of pending oplogs in terms of # of rows. Default
  // -1 means all rows.
  int64_t num_oplog_rows{-1};
  // kDenseRowOpLog or kSparseRowOpLog
  int oplog_type{RowOpLogType::kDenseRowOpLog};
};

class PsApplication {
public:
  void Run(int num_worker_threads) {
    //google::InitGoogleLogging("PsApp");

    process_barrier_.reset(new boost::barrier(num_worker_threads));

    // Step 1.0. Register common row types
    PSTableGroup::RegisterRow<
      DenseRow<float>>(kDenseFloatRow);
    PSTableGroup::RegisterRow<
      DenseRow<int32_t>>(kDenseIntRow);
    PSTableGroup::RegisterRow<
      SparseRow<float>>(kSparseFloatRow);
    PSTableGroup::RegisterRow<
      SparseRow<int>>(kSparseIntRow);

    std::vector<TableConfig> configs = ConfigTables();

    // Step 1.1. Initialize Table Group
    TableGroupConfig table_group_config;
    table_group_config.num_tables = configs.size();
    GetHostInfos(FLAGS_hostfile, &table_group_config.host_map);
    table_group_config.client_id = FLAGS_client_id;
    // +1 to include the main() thread
    table_group_config.num_local_app_threads = num_worker_threads + 1;
    // Consistency model
    table_group_config.consistency_model = SSPPush;
    // False to disallow table access for the main thread.
    PSTableGroup::Init(table_group_config, false);

    InitApp();
    // Create Tables
    for (int i = 0; i < configs.size(); ++i) {
      auto config = ConvertTableConfig(configs[i]);
      PSTableGroup::CreateTable(i, config);
      table_names_[configs[i].name] = i;
    }
    PSTableGroup::CreateTableDone();

    // Spin num_worker_threads to run.
    LOG(INFO) << "Starting program with " << num_worker_threads << " threads "
      << "on client " << table_group_config.client_id;
    std::vector<std::thread> threads(num_worker_threads);
    for (auto& thr : threads) {
      thr = std::thread(&PsApplication::RunWorkerThread, this);
    }
    for (auto& thr : threads) {
      thr.join();
    }

    // Shutdown PSTableGroup
    PSTableGroup::ShutDown();
    LOG(INFO) << "Program finished and shut down!";
  }

protected:
  PsApplication() : thread_counter_(0) {};

  ~PsApplication() {};

  // InitApp() can data loading in single-thread. Or it can do nothing. It may
  // not access PS table.
  virtual void InitApp() = 0;

  // Return a set of table configuration. See TableConfig struct.
  virtual std::vector<TableConfig> ConfigTables() = 0;

  virtual void WorkerThread(int threadId) = 0;

  template<typename V>
  Table<V> GetTable(const std::string& table_name) {
    auto it = table_names_.find(table_name);
    CHECK(it != table_names_.cend()) << "Table " << table_name
      << " was not registered";
    return PSTableGroup::GetTableOrDie<float>(it->second);
  }

private:
  void RunWorkerThread() {
    PSTableGroup::RegisterThread();
    int thread_id = thread_counter_++;
    WorkerThread(thread_id);
    PSTableGroup::DeregisterThread();
  }

  // Convert from a simplified TableConfig to ClientTableConfig
  ClientTableConfig ConvertTableConfig(const TableConfig& config) {
    // Check required fields in TableConfig
    CHECK_NE("", config.name) << "Table name must be set";
    CHECK_NE(-1, config.num_rows)
      << "Table " << config.name << " num_rows must be set.";
    CHECK_NE(-1, config.num_cols)
      << "Table " << config.name << " num_cols must be set.";
    CHECK_NE(kUndefinedRow, config.row_type)
      << "Table " << config.name << " row_type must be set.";

    ClientTableConfig table_config;
    table_config.table_info.table_staleness = config.staleness;
    table_config.table_info.row_type = config.row_type;
    table_config.table_info.row_capacity = config.num_cols;

    table_config.process_cache_capacity = config.num_rows_to_cache == -1 ?
      config.num_rows : config.num_rows_to_cache;
    table_config.table_info.row_oplog_type = config.oplog_type;
    table_config.oplog_capacity = config.num_oplog_rows == -1 ?
      config.num_rows : config.num_oplog_rows;
    table_config.table_info.dense_row_oplog_capacity =
      table_config.table_info.row_capacity;
    return table_config;
  }

protected:
  boost::scoped_ptr<boost::barrier> process_barrier_;

private:
  std::atomic<int> thread_counter_;

  // Map table name to PS's internal table ID.
  std::map<std::string, int> table_names_;
};
}  // namespace petuum
