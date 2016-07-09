// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.10.04
// Modified by: Binghong Chen (2016.06.30)

#include "mlr_engine.hpp"
#include "mlr_app.cpp"
#include "common.hpp"
#include <petuum_ps_common/include/petuum_ps.hpp>
#include <gflags/gflags.h>
#include <thread>
#include <vector>
#include <cstdint>
#include <algorithm>

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
DEFINE_int32(num_train_data, 0, "Number of training data. Cannot exceed the "
    "number of data in train_file. 0 to use all train data.");
DEFINE_string(train_file, "", "The program expects 2 files: train_file, "
    "train_file.meta. If global_data = false, then it looks for train_file.X, "
    "train_file.X.meta, where X is the client_id.");
DEFINE_bool(global_data, false, "If true, all workers read from the same "
    "train_file. If false, append X. See train_file.");
DEFINE_string(test_file, "", "The program expects 2 files: test_file, "
    "test_file.meta, test_file must have format specified in read_format "
    "flag. All clients read test file if FLAGS_perform_test == true.");
DEFINE_int32(num_train_eval, 20, "Use the next num_train_eval train data "
    "(per thread) for intermediate eval.");
DEFINE_int32(num_test_eval, 20, "Use the first num_test_eval test data for "
    "intermediate eval. 0 for using all. The final eval will always use all "
    "test data.");
DEFINE_bool(perform_test, false, "Ignore test_file if true.");
DEFINE_bool(use_weight_file, false, "True to use init_weight_file as init");
DEFINE_string(weight_file, "", "Use this file to initialize weight. "
  "Format of the file is libsvm (see SaveWeight in MLRSGDSolver).");

// MLR Parameters
DEFINE_int32(num_epochs, 1, "Number of data sweeps.");
DEFINE_int32(num_batches_per_epoch, 10, "Since we Clock() at the end of each batch, "
    "num_batches_per_epoch is effectively the number of clocks per epoch (iteration)");
DEFINE_double(init_lr, 0.1, "Initial learning rate");
DEFINE_double(lr_decay_rate, 1, "multiplicative decay");
DEFINE_int32(num_batches_per_eval, 10, "Number of batches per evaluation");
DEFINE_double(lambda, 0, "L2 regularization parameter.");

// Misc
DEFINE_string(output_file_prefix, "", "Results go here.");
DEFINE_int32(w_table_id, 0, "Weight table's ID in PS.");
DEFINE_int32(loss_table_id, 1, "Loss table's ID in PS.");
DEFINE_int32(staleness, 0, "staleness for weight tables.");
DEFINE_int32(row_oplog_type, petuum::RowOpLogType::kDenseRowOpLog,
    "row oplog type");
DEFINE_bool(oplog_dense_serialized, false, "True to not squeeze out the 0's "
    "in dense oplog.");
DEFINE_int32(num_secs_per_checkpoint, 600, "# of seconds between each saving "
    "to disk");
DEFINE_int32(w_table_num_cols, 1000000,
    "# of columns in w_table. Only used for binary LR.");


int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  
  MLRApp mlrapp;
  mlrapp.run();
  
  return 0;
}