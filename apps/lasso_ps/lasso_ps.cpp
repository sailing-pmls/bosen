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
/*
 * Coordinate descent-based Lasso application built on Petuum Parameter Server.
 * This is implementation of parallel Lasso in:
 * Ho, Qirong, et al. "More effective distributed ml via a stale synchronous parallel parameter server." Advances in Neural Information Processing Systems. 2013.
 *
 * Author: seunghak
 */
#include <vector>
#include <fstream>

#include <petuum_ps/include/petuum_ps.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <pthread.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/random/variate_generator.hpp>

#include "spmat.hpp"

// Command-line flags
DEFINE_string(hostfile, "", "Path to Petuum PS server configuration file");
DEFINE_string(datafile_X, "", "Input X");
DEFINE_string(datafile_Y, "", "Output Y");
DEFINE_string(output_prefix, "", "Output results with this prefix. If the prefix is X, the output will be called X.BETA");
DEFINE_int32(num_rows, 1000, "Number of Rows in X");
DEFINE_int32(num_cols, 1000, "Number of Columns in X");
DEFINE_int32(num_clients, 1, "Total number of clients");
DEFINE_int32(num_worker_threads, 1, "Number of worker threads per client");
DEFINE_int32(client_id, 0, "This client's ID");
DEFINE_double(lambda, 0.01, "Lambda");
DEFINE_int32(num_iterations, 100, "Number of iterations");
DEFINE_int32(staleness, 0, "Staleness");

// Typedefs
typedef std::pair<int,float> int_pair_float_t;

// Data variables
col_spmat Xcol(FLAGS_num_rows,FLAGS_num_cols);
col_spmat Ycol(FLAGS_num_rows,1);
col_spmat residual(FLAGS_num_rows,1);

// Returns the number of workers threads across all clients
int get_total_num_workers() {
  return FLAGS_num_clients * FLAGS_num_worker_threads;
}
// Returns the global thread ID of this worker
int get_global_worker_id(int local_thread_id) {
  return (FLAGS_client_id * FLAGS_num_worker_threads) + local_thread_id;
}

void standardization_X() {
  float mean_val = 0;
  float sq_val = 0;
  int row_cnt = 0;
  int row_index = 0;
  float tmp = 0;
  float xvalue = 0;

  for (int i=0; i<FLAGS_num_cols; ++i){
    mean_val = 0;
    row_cnt = 0;
    for (auto p : Xcol.col(i)){
      xvalue = p.second;
      mean_val += xvalue; 
      row_cnt += 1;
    }
    if (row_cnt>0){
      sq_val = 0; 
      mean_val = mean_val / row_cnt;
      for (auto p : Xcol.col(i)){
        row_index = p.first;
        xvalue = p.second;
	tmp = xvalue - mean_val;
	Xcol(row_index,i) = tmp;
        sq_val += tmp*tmp;
      } 
      sq_val = sqrt(sq_val);
      if (sq_val>0){
        for (auto p : Xcol.col(i)){
          row_index = p.first;
          xvalue = p.second;
	  Xcol(row_index,i) = xvalue / sq_val;
        }
      }
    }
  }
}

void standardization_Y() {
  float mean_val = 0;
  float sq_val = 0;
  int row_cnt = 0;
  int row_index = 0;
  float tmp = 0;
  float xvalue = 0;
  for (auto p : Ycol.col(0)){
    xvalue = p.second;
    mean_val += xvalue; 
    row_cnt += 1;
  }

  if (row_cnt>0){
    sq_val = 0; 
    mean_val = mean_val / row_cnt;
    for (auto p : Ycol.col(0)){
      row_index = p.first;
      xvalue = p.second;
      tmp = xvalue - mean_val;
      Ycol(row_index,0) = tmp;
      sq_val += tmp*tmp;
    } 
    sq_val = sqrt(sq_val);
    if (sq_val>0){
      for (auto p : Ycol.col(0)){
        row_index = p.first;
        xvalue = p.second;
	Ycol(row_index,0) = xvalue / sq_val;
      }
    }
  }
}

void read_sparse_matrix_X(std::string inputfile) {
  Xcol.resize(FLAGS_num_rows,FLAGS_num_cols);
  std::ifstream inputstream(inputfile.c_str());
  while(true) {
    int row, col;
    float val;
    inputstream >> row >> col >> val;
    if (!inputstream) {
      break;
    }
    Xcol(row,col) = val;
  }
  inputstream.close();
  // standardize X. It makes the mean is zero, and 
  // variance is 1 for each column
  standardization_X();
}

// Read sparse data matrix into Xcol, Ycol. Each line of the matrix
// is a whitespace-separated triple (row,col,value), where row>=0 and col>=0.
// For example:
//
// 0 0 0.5
// 1 2 1.5
// 2 1 2.5
//
// This specifies a 3x3 matrix with 3 nonzero elements: 0.5 at (0,0), 1.5 at
// (1,2) and 2.5 at (2,1).

void read_sparse_matrix_Y(std::string inputfile) {
  Ycol.resize(FLAGS_num_rows,1);
  residual.resize(FLAGS_num_rows,1);

  std::ifstream inputstream(inputfile.c_str());
  while(true) {
    int row, col;
    float val;
    inputstream >> row >> col >> val;
    if (!inputstream) {
      break;
    }
    Ycol(row,0) = val;
  }
  inputstream.close();
  // standardize Y. It makes the mean is zero, and 
  // variance is 1 for each column
  standardization_Y();
}

void lasso_element(int a, int iter, int global_worker_id,
                 petuum::Table<float>& beta_table) {
  // get beta value
  petuum::RowAccessor beta_acc;
  beta_table.Get(0, &beta_acc);
  const petuum::DenseRow<float>& beta_val = beta_acc.Get<petuum::DenseRow<float> >();

  float new_beta = 0.0;
  float xvalue = 0.0;
  int row_index = 0;

  // Compute value for updating beta(a)
  float val_before_thr = 0.0;
  for (auto p : Xcol.col(a)){
    row_index = p.first;
    xvalue = p.second;
    val_before_thr += xvalue * residual.get(row_index,0);
  }
  val_before_thr += beta_val[a]; 
  // soft thresdholing
  if (val_before_thr>0 && FLAGS_lambda<fabs(val_before_thr)){
    new_beta = val_before_thr - FLAGS_lambda;
  } else if (val_before_thr<0 && FLAGS_lambda<fabs(val_before_thr)){
    new_beta = val_before_thr + FLAGS_lambda;
  } else {
    new_beta = 0; 
  }
  // Update residual
  for (auto p : residual.col(0)){
    residual(p.first,0) = p.second + Xcol.get(p.first,a) * (beta_val[a] - new_beta);
  }
  // Commit updates to Petuum PS
  beta_table.Inc(0,a,-beta_val[a]+new_beta);
}


// Outputs beta_table
void output_to_disk(petuum::Table<float>& beta_table) {
  std::string beta_file = FLAGS_output_prefix + ".BETA";
  std::ofstream beta_stream(beta_file.c_str());
  petuum::RowAccessor beta_acc;
  beta_table.Get(0, &beta_acc);
  const petuum::DenseRow<float>& beta_val = beta_acc.Get<petuum::DenseRow<float> >();
  for (uint32_t i = 0; i < FLAGS_num_rows; ++i) {
    beta_stream << beta_val[i] << "\n";
  }
  beta_stream.close();
}
void* solve_lasso(void* args) {
  int local_thread_id = *reinterpret_cast<int*>(args);
  // Register this thread with Petuum PS
  petuum::TableGroup::RegisterThread();
  // Get tables
  petuum::Table<float> beta_table = petuum::TableGroup::GetTableOrDie<float>(0);
  // Initialize MF solver
  const int total_num_workers = get_total_num_workers();
  const int global_worker_id = get_global_worker_id(local_thread_id); // global_worker_id lies in the range [0,total_num_workers)

  // Initialize beta table and residual
  petuum::RowAccessor beta_acc;
  beta_table.Get(0, &beta_acc);
  const petuum::DenseRow<float>& beta_val = beta_acc.Get<petuum::DenseRow<float> >();
  for (int i = 0; i < FLAGS_num_cols; ++i) {
    beta_table.Inc(0,i,-beta_val[i]);
  }
  for (int i=0; i<FLAGS_num_rows; ++i){
    residual(i,0) = Ycol.get(i,0);
  } 

  // Run additional iterations to let stale values finish propagating
  for (int iter = 0; iter < FLAGS_staleness; ++iter) {
    petuum::TableGroup::Clock();
  }

  float obj = 0;
  // Run lasso solver
  for (int iter = 0; iter < FLAGS_num_iterations; ++iter) {
    boost::posix_time::ptime beginT = boost::posix_time::microsec_clock::local_time();
    if (global_worker_id == 0) {
      std::cout << "Iteration " << iter+1 << "/" << FLAGS_num_iterations << "... " << std::flush;
    }
    // Output loss function
    if (global_worker_id == 0) {
      petuum::RowAccessor beta_acc;
      beta_table.Get(0, &beta_acc);
      const petuum::DenseRow<float>& beta_val = beta_acc.Get<petuum::DenseRow<float> >();
      obj=0;
      for (auto p : residual.col(0)){
	  obj=obj+pow(p.second,2);
      }
      obj = obj/2;
      for (int col_index=0; col_index < FLAGS_num_cols; ++col_index){
	obj += FLAGS_lambda*fabs(beta_val[col_index]);
      }
      std::cout << " loss function = " << obj << "... " << std::flush;
    }
    // Divide beta coefficients elements across workers, and perform coordinate descent
    for (int a = global_worker_id; a < FLAGS_num_cols; a += total_num_workers) {
      lasso_element(a,iter,global_worker_id,beta_table);
    }
    // Advance Parameter Server iteration
    petuum::TableGroup::Clock();
    boost::posix_time::time_duration elapTime = boost::posix_time::microsec_clock::local_time() - beginT;
    if (global_worker_id == 0) {
      std::cout << "elapsed time = " << ((float) elapTime.total_milliseconds()) / 1000 << std::endl;
    }
  }
  // Run additional iterations to let stale values finish propagating
  for (int iter = 0; iter < FLAGS_staleness; ++iter) {
    petuum::TableGroup::Clock();
  }
  // Output results to disk
  if (global_worker_id == 0) {
    std::cout << "Outputting results to prefix " << FLAGS_output_prefix << " ... " << std::flush;
    output_to_disk(beta_table);
    std::cout << "done" << std::endl;
  }
  // Deregister this thread with Petuum PS
  petuum::TableGroup::DeregisterThread();
  return 0;
}


// Main function
int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  boost::posix_time::ptime beginT = boost::posix_time::microsec_clock::local_time();
  
  // Configure Petuum PS
  petuum::TableGroupConfig table_group_config;
  // Global params
  table_group_config.num_total_server_threads = FLAGS_num_clients;  // 1 server thread per client
  table_group_config.num_total_bg_threads = FLAGS_num_clients;  // 1 background thread per client
  table_group_config.num_total_clients = FLAGS_num_clients;
  table_group_config.num_tables = 1;  // beta_table
  // Local params
  table_group_config.num_local_server_threads = 1;
  table_group_config.num_local_bg_threads = 1;
  table_group_config.num_local_app_threads = FLAGS_num_worker_threads + 1;  // +1 for main() thread
  petuum::GetHostInfos(FLAGS_hostfile, &table_group_config.host_map);
  petuum::GetServerIDsFromHostMap(&table_group_config.server_ids, table_group_config.host_map);
  table_group_config.client_id = FLAGS_client_id;
  const uint32_t threadspace_per_process = 1000;
  table_group_config.local_id_min = FLAGS_client_id * threadspace_per_process;
  table_group_config.local_id_max = (FLAGS_client_id+1) * threadspace_per_process - 1;
  table_group_config.consistency_model = petuum::SSP;
  // Configure PS row types
  petuum::TableGroup::RegisterRow<petuum::DenseRow<float> >(0);  // Register dense rows as ID 0
  // Start PS
  // IMPORTANT: This command starts up the name node service on client 0.
  //            We therefore do it ASAP, before other lengthy actions like
  //            loading data.
  petuum::TableGroup::Init(table_group_config, false);  // Initializing thread does not need table access
  
  // Load Data
  if (FLAGS_client_id == 0) {
    std::cout << "Loading data... " << std::flush;
  }

  if (FLAGS_client_id == 0) {
    std::cout << "done" << std::endl;
    // Output statistics
    std::cout << "num_rows: " << FLAGS_num_rows << std::endl;
    std::cout << "num_cols: " << FLAGS_num_cols << std::endl;
    std::cout << "input X: " << FLAGS_datafile_X << std::endl;
    std::cout << "input Y: " << FLAGS_datafile_Y << std::endl;
    std::cout << "lambda: " << FLAGS_lambda << std::endl;
    std::cout << "# number of iterations: " << FLAGS_num_iterations << std::endl;
    std::cout << "# client machines: " << FLAGS_num_clients << std::endl;
    std::cout << "# worker threads per client: " << FLAGS_num_worker_threads << std::endl;
    std::cout << "SSP staleness: " << FLAGS_staleness << std::endl;
  }
  read_sparse_matrix_X(FLAGS_datafile_X);
  read_sparse_matrix_Y(FLAGS_datafile_Y);
  
  // Configure PS tables
  petuum::ClientTableConfig table_config;
  table_config.table_info.row_type = 0; // Dense rows
  table_config.oplog_capacity = 100;
  // beta table (1 by num_cols)
  table_config.table_info.table_staleness = FLAGS_staleness;
  table_config.table_info.row_capacity = FLAGS_num_cols;
  table_config.process_cache_capacity = FLAGS_num_cols; 
  petuum::TableGroup::CreateTable(0,table_config);
  // Finished creating tables
  petuum::TableGroup::CreateTableDone();
  
  // Run Petuum PS-based Lasso solver
  pthread_t* threads = new pthread_t[FLAGS_num_worker_threads];
  int* local_thread_ids = new int[FLAGS_num_worker_threads];
  for (int t = 0; t < FLAGS_num_worker_threads; ++t) {
    local_thread_ids[t] = t;
    pthread_create(threads+t, NULL, solve_lasso, local_thread_ids+t);
  }
  petuum::TableGroup::WaitThreadRegister();
  for (int t = 0; t < FLAGS_num_worker_threads; ++t) {
    pthread_join(threads[t], NULL);
  }
  delete threads;
  delete local_thread_ids;
  
  // Cleanup and output runtime
  petuum::TableGroup::ShutDown();
  boost::posix_time::time_duration elapTime = boost::posix_time::microsec_clock::local_time() - beginT;
  if (FLAGS_client_id == 0) {
    std::cout << "total runtime = " << ((float) elapTime.total_milliseconds()) / 1000 << "s" << std::endl;
  }
  return 0;
}
