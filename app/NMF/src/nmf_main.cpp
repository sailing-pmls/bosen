#include <petuum_ps_common/include/petuum_ps.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <vector>
#include <thread>

#include "NMFEngine.hpp"
#include "util/context.hpp"

/* Petuum Parameters */
DEFINE_string(hostfile, "", "Path to file containing server ip:port.");
DEFINE_int32(num_clients, 1, "Total number of clients");
DEFINE_int32(num_worker_threads, 4, "Number of app threads in this client");
DEFINE_int32(client_id, 0, "Client ID");
DEFINE_int32(num_comm_channels_per_client, 4, 
        "number of comm channels per client");

// Test parameters
DEFINE_int32(number_rows, 0, "Number of rows");
DEFINE_int32(row_size, 0, "Size of a row");
DEFINE_int32(table_staleness, 0, "Staleness for R table."
        "Default value is 0.");

/* No need to change the following */
DEFINE_string(stats_path, "", "Statistics output file.");
DEFINE_string(consistency_model, "SSPPush", "SSP or SSPPush or ...");
DEFINE_int32(row_oplog_type, petuum::RowOpLogType::kDenseRowOpLog, 
        "row oplog type");
DEFINE_bool(oplog_dense_serialized, true, "dense serialized oplog");
DEFINE_string(process_storage_type, "BoundedDense", "process storage type");

std::atomic<int> thread_counter;

// run a simple update
void Run() {
	// register thread
	int thread_id = thread_counter++;
	petuum::PSTableGroup::RegisterThread();
	LOG(INFO) << "client " << FLAGS_client_id << ", thread " 
		<< thread_id << " registers!";

	// get table
	petuum::Table<float> table = petuum::PSTableGroup::GetTableOrDie<float>(0);
	petuum::PSTableGroup::GlobalBarrier();


	// simple update: increase all parameters by 1
	LOG(INFO) << "Update " << FLAGS_number_rows << " rows of size " << FLAGS_row_size;
	for (int row_id = 0; row_id < FLAGS_number_rows; ++row_id) {
		petuum::UpdateBatch<float> update;
		for (int col_id = 0; col_id < FLAGS_row_size; ++col_id) {
			update.Update(col_id, 1);
		}
		table.BatchInc(row_id, update);
	}

	// send updates
	petuum::PSTableGroup::Clock();
	LOG(INFO) << "Clock returned. ";

	// shut down
	petuum::PSTableGroup::GlobalBarrier();
	petuum::PSTableGroup::DeregisterThread();
}


int main(int argc, char * argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    google::InitGoogleLogging(argv[0]);

    petuum::TableGroupConfig table_group_config;
    table_group_config.num_comm_channels_per_client
      = FLAGS_num_comm_channels_per_client;
    table_group_config.num_total_clients = FLAGS_num_clients;
    // Dictionary table and loss table
    table_group_config.num_tables = 1;
    // + 1 for main()
    table_group_config.num_local_app_threads = FLAGS_num_worker_threads + 1;;
    table_group_config.client_id = FLAGS_client_id;
    
    petuum::GetHostInfos(FLAGS_hostfile, &table_group_config.host_map);
    if (FLAGS_consistency_model == "SSP") {
        table_group_config.consistency_model = petuum::SSP;
    } else if (FLAGS_consistency_model == "SSPPush") {
        table_group_config.consistency_model = petuum::SSPPush;
    } else if (FLAGS_consistency_model == "LocalOOC") {
        table_group_config.consistency_model = petuum::LocalOOC;
    } else {
        LOG(FATAL) << "Unknown consistency model: " << FLAGS_consistency_model;
    }

    petuum::ProcessStorageType process_storage_type;
    if (FLAGS_process_storage_type == "BoundedDense") {
        process_storage_type = petuum::BoundedDense;
    } else if (FLAGS_process_storage_type == "BoundedSparse") {
        process_storage_type = petuum::BoundedSparse;
    } else {
        LOG(FATAL) << "Unknown process storage type " << FLAGS_process_storage_type;
    }

    // Stats
    table_group_config.stats_path = FLAGS_stats_path;
    // Configure row types
    petuum::PSTableGroup::RegisterRow<petuum::DenseRow<float> >(0);

    // Start PS
    petuum::PSTableGroup::Init(table_group_config, false);


    // Create PS table
    petuum::ClientTableConfig table_config;
    table_config.table_info.row_type = 0;
    table_config.table_info.table_staleness = FLAGS_table_staleness;
    table_config.table_info.row_capacity = FLAGS_row_size;
    // Assume all rows put into memory
    table_config.process_cache_capacity = FLAGS_number_rows;
    table_config.table_info.row_oplog_type = FLAGS_row_oplog_type;
    table_config.table_info.oplog_dense_serialized = 
        FLAGS_oplog_dense_serialized;
    table_config.table_info.dense_row_oplog_capacity = 
        table_config.table_info.row_capacity;
    table_config.thread_cache_capacity = 1;
    table_config.oplog_capacity = table_config.process_cache_capacity;
    table_config.process_storage_type = process_storage_type;

    CHECK(petuum::PSTableGroup::CreateTable(0, table_config)) 
        << "Failed to create R table";


    petuum::PSTableGroup::CreateTableDone();
    LOG(INFO) << "Create Table Done!";

    std::vector<std::thread> threads(FLAGS_num_worker_threads);
    for (auto & thr: threads) {
        thr = std::thread(Run);
    }
    for (auto & thr: threads) {
        thr.join();
    }
    petuum::PSTableGroup::ShutDown();
    LOG(INFO) << "NMF shut down!";
    return 0;
}
