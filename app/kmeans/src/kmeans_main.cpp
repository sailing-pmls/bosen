//Part of the code is inspired by Dai Weis code for MLR and kMeans from sofia written by D. Sculley.
/*
 * KMeansMain.cpp
 *
 *  Created on: Oct 25, 2014
 *      Author: manu
 */

#include <petuum_ps_common/include/petuum_ps.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <thread>
#include <vector>
#include <cstdint>
#include "kmeans.h"
#include "cluster_centers.h"
#include "kmeans_methods.h"

// Petuum Parameters
DEFINE_string(hostfile, "", "Path to file containing server ip:port.");
DEFINE_int32(num_clients, 1, "Total number of clients");
DEFINE_int32(num_app_threads, 1, "Number of app threads in this client");
DEFINE_int32(client_id, 0, "Client ID");
DEFINE_string(consistency_model, "SSPPush", "SSP or SSPPush");
DEFINE_string(stats_path, "", "Statistics output file");
DEFINE_int32(num_comm_channels_per_client, 1,
		"number of comm channels per client");

// Data Parameters
DEFINE_int32(num_centers, 100, "Number of centers");
DEFINE_bool(isSparse, false, "If the data is Sparse");
DEFINE_int32(dimensionality, 100, "dimensionality of the data");
DEFINE_int32(num_train_data, 100, "Number of training data. Cannot exceed the "
		"number of data in train_file. 0 to use all train data.");
//todo decide upon the data format.
DEFINE_string(train_file, "",
		"The program expects 2 files: train_file, "
				"train_file.meta. If global_data = false, then it looks for train_file.X, "
				"train_file.X.meta, where X is the client_id. train_file must have format "
				"specified in read_format flag.");

DEFINE_bool(perform_test, false, "Ignore test_file if true.");


// KMeans Parameters
DEFINE_int32(num_epochs, 1, "Number of iterations");

DEFINE_int32(mini_batch_size, 1000, "The size of the mini batch");

DEFINE_bool(load_clusters_from_disk,false,"Load centers from external source.");
DEFINE_string(cluster_centers_input_location,"","location for file containing initial centers");

//todo decide on the following
DEFINE_double(learning_rate, 0.1, "Learning rate for miniBatch itertions");

// Misc
DEFINE_string(output_file_prefix, "", "Results go here.");

//petuum tables.
DEFINE_int32(centres_table_id, 0, "Weight table's ID in PS.");
DEFINE_int32(update_centres_table_id, 3, "Delta Weight table's ID in PS. This is used to accumulate changes and then apply them.");
DEFINE_int32(center_count_tableId, 1, "center count's ID in PS.");
DEFINE_int32(objective_function_value_tableId, 4, "objective function value table id");

DEFINE_int32(staleness, 0, "staleness for centers tables.");
DEFINE_int32(count_table_staleness, 0, "staleness for center count table.");
DEFINE_int32(objective_table_stalesness,0,"staleness for objective function table");
DEFINE_int32(update_centres_table_stalenss,0,"staleness for the table which hold the ith epochs number of centers");


DEFINE_int32(row_oplog_type, petuum::RowOpLogType::kSparseRowOpLog,
		"row oplog type");
DEFINE_bool(oplog_dense_serialized, false, "True to not squeeze out the 0's "
		"in dense oplog.");
DEFINE_int32(num_secs_per_checkpoint, 600, "# of seconds between each saving "
		"to disk");
DEFINE_int32(total_num_of_training_samples, 100, "Total number of training samples");

const int32_t kDenseRowFloatTypeID = 0;
const int32_t kDenseRowIntTypeID = 1;

int main(int argc, char *argv[]) {
	google::ParseCommandLineFlags(&argc, &argv, true);
	google::InitGoogleLogging(argv[0]);

	petuum::HighResolutionTimer data_loading_timer;
	LOG(INFO)<< "training file location: " << FLAGS_train_file;
	KMeans kmeans;
	kmeans.ReadData();
	LOG(INFO)<< "Data Loading Complete. Loaded "  << kmeans.GetTrainingDataSize() << " in "  <<data_loading_timer.elapsed();

	//  kmeans.

	petuum::TableGroupConfig table_group_config;
	table_group_config.num_comm_channels_per_client =
			FLAGS_num_comm_channels_per_client;
	table_group_config.num_total_clients = FLAGS_num_clients;

	table_group_config.num_tables = 4;
	//  // + 1 for main() thread.
	table_group_config.num_local_app_threads = FLAGS_num_app_threads + 1;
	table_group_config.client_id = FLAGS_client_id;
	table_group_config.stats_path = FLAGS_stats_path;
	//
	petuum::GetHostInfos(FLAGS_hostfile, &table_group_config.host_map);
	if (std::string("SSP").compare(FLAGS_consistency_model) == 0) {
		table_group_config.consistency_model = petuum::SSP;
	} else if (std::string("SSPPush").compare(FLAGS_consistency_model) == 0) {
		table_group_config.consistency_model = petuum::SSPPush;
	} else if (std::string("LocalOOC").compare(FLAGS_consistency_model) == 0) {
		table_group_config.consistency_model = petuum::LocalOOC;
	} else {
		LOG(FATAL)<< "Unkown consistency model: " << FLAGS_consistency_model;
	}

	petuum::PSTableGroup::RegisterRow<petuum::DenseRow<float> >(
			kDenseRowFloatTypeID);
	petuum::PSTableGroup::RegisterRow<petuum::DenseRow<int> >(
			kDenseRowIntTypeID);
	//

	petuum::PSTableGroup::Init(table_group_config, false);
	//

	petuum::ClientTableConfig table_config;
	table_config.table_info.row_type = kDenseRowFloatTypeID;
	table_config.table_info.table_staleness = FLAGS_staleness;
	//  //table_config.table_info.row_capacity = feature_dim * num_labels;
	table_config.table_info.row_capacity = FLAGS_dimensionality;
	table_config.table_info.row_oplog_type = FLAGS_row_oplog_type;
	table_config.table_info.oplog_dense_serialized =
			FLAGS_oplog_dense_serialized;
	table_config.table_info.dense_row_oplog_capacity = FLAGS_dimensionality;
	//  //table_config.process_cache_capacity = 1;
	table_config.process_cache_capacity = FLAGS_num_centers;
	table_config.oplog_capacity = table_config.process_cache_capacity;
	petuum::PSTableGroup::CreateTable(FLAGS_centres_table_id, table_config);

	LOG(INFO) << "created centers table";


	//Objective Function table.
	table_config.table_info.dense_row_oplog_capacity = FLAGS_num_epochs+1;
	table_config.table_info.table_staleness = 0;
	petuum::PSTableGroup::CreateTable(FLAGS_objective_function_value_tableId, table_config);
	LOG(INFO) << "created objective values table";


	// Centers table
	table_config.table_info.row_type = kDenseRowIntTypeID;
	table_config.table_info.table_staleness = FLAGS_count_table_staleness;
	table_config.table_info.row_capacity = FLAGS_num_centers;
	table_config.process_cache_capacity = 1000;
	table_config.oplog_capacity = table_config.process_cache_capacity;
	petuum::PSTableGroup::CreateTable(FLAGS_center_count_tableId, table_config);


	// Table to hold the local deltas.
	petuum::PSTableGroup::CreateTable(FLAGS_update_centres_table_id, table_config);


	LOG(INFO) << "Completed creating tables" ;

	petuum::PSTableGroup::CreateTableDone();

	std::vector<std::thread> threads(FLAGS_num_app_threads);
	for (auto& thr : threads) {

		thr = std::thread(&KMeans::Start, std::ref(kmeans));
	}
	for (auto& thr : threads) {
		thr.join();
	}

	petuum::PSTableGroup::ShutDown();
	LOG(INFO)<< "Kmeans finished and shut down!";
	return 0;
}
