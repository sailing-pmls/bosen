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
 * Least-squares Matrix Factorization application built on Petuum Parameter Server.
 *
 * Author: qho
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

// Command-line flags
DEFINE_string(hostfile, "", "Path to Petuum PS server configuration file");
DEFINE_string(datafile, "", "Input sparse matrix");
DEFINE_string(output_prefix, "", "Output results with this prefix. If the prefix is X, the output will be called X.L and X.R");
DEFINE_double(init_step_size, 0.5, "Initial stochastic gradient descent step size");
DEFINE_int32(rng_seed, 967234, "RNG seed for initializing factor matrices L, R");
DEFINE_int32(num_clients, 1, "Total number of clients");
DEFINE_int32(num_worker_threads, 1, "Number of worker threads per client");
DEFINE_int32(client_id, 0, "This client's ID");
DEFINE_int32(K, 100, "Factorization rank");
DEFINE_int32(num_iterations, 100, "Number of iterations");
DEFINE_int32(staleness, 0, "Staleness");

// Typedefs
typedef std::pair<int,float> int_pair_float_t;

// Data variables
int N_, M_; // Number of rows and cols
std::vector<int> X_row; // Row index of each nonzero entry in the data matrix
std::vector<int> X_col; // Column index of each nonzero entry in the data matrix
std::vector<float> X_val; // Value of each nonzero entry in the data matrix

// Returns the number of workers threads across all clients
int get_total_num_workers() {
  return FLAGS_num_clients * FLAGS_num_worker_threads;
}
// Returns the global thread ID of this worker
int get_global_worker_id(int local_thread_id) {
  return (FLAGS_client_id * FLAGS_num_worker_threads) + local_thread_id;
}

// Read sparse data matrix into X_row, X_col and X_val. Each line of the matrix
// is a whitespace-separated triple (row,col,value), where row>=0 and col>=0.
// For example:
//
// 0 0 0.5
// 1 2 1.5
// 2 1 2.5
//
// This specifies a 3x3 matrix with 3 nonzero elements: 0.5 at (0,0), 1.5 at
// (1,2) and 2.5 at (2,1).
void read_sparse_matrix(std::string inputfile) {
  X_row.clear();
  X_col.clear();
  X_val.clear();
  N_ = 0;
  M_ = 0;
  std::ifstream inputstream(inputfile.c_str());
  while(true) {
    int row, col;
    float val;
    inputstream >> row >> col >> val;
    if (!inputstream) {
      break;
    }
    X_row.push_back(row);
    X_col.push_back(col);
    X_val.push_back(val);
    N_ = row+1 > N_ ? row+1 : N_;
    M_ = col+1 > M_ ? col+1 : M_;
  }
  inputstream.close();
}

// Performs stochastic gradient descent on X_row[a], X_col[a], X_val[a].
void sgd_element(int a, int iter, int global_worker_id,
                 petuum::Table<float>& L_table,
                 petuum::Table<float>& R_table,
                 petuum::Table<float>& loss_table) {
  // Let i = X_row[a], j = X_col[a], and X(i,j) = X_val[a]
  const int i = X_row[a];
  const int j = X_col[a];
  const float Xij = X_val[a];
  // Read L(i,:) and R(:,j) from Petuum PS
  petuum::RowAccessor Li_acc;
  petuum::RowAccessor Rj_acc;
  L_table.Get(i, &Li_acc);
  R_table.Get(j, &Rj_acc);
  const petuum::DenseRow<float>& Li = Li_acc.Get<petuum::DenseRow<float> >();
  const petuum::DenseRow<float>& Rj = Rj_acc.Get<petuum::DenseRow<float> >();
  
  // Compute L(i,:) * R(:,j)
  float LiRj = 0.0;
  for (int k = 0; k < FLAGS_K; ++k) {
    LiRj += Li[k] * Rj[k];
  }
  // Update the loss function
  loss_table.Inc(0,global_worker_id,pow(Xij - LiRj, 2));
  
  // Now update L(i,:) and R(:,j) based on the loss function at X(i,j).
  // The loss function at X(i,j) is ( X(i,j) - L(i,:)*R(:,j) )^2.
  //
  // The gradient w.r.t. L(i,k) is -2*X(i,j)R(k,j) + 2*L(i,:)*R(:,j)*R(k,j).
  // The gradient w.r.t. R(k,j) is -2*X(i,j)L(i,k) + 2*L(i,:)*R(:,j)*L(i,k).
  float step_size = FLAGS_init_step_size * pow(100.0 + iter,-0.5);
  std::vector<float> Li_update(FLAGS_K);
  std::vector<float> Rj_update(FLAGS_K);
  for (int k = 0; k < FLAGS_K; ++k) {
    float gradient = 0.0;
    // Compute update for L(i,k)
    gradient = (-2 * Xij * Rj[k]) + (2 * LiRj * Rj[k]);
    Li_update[k] = -gradient * step_size;
    // Compute update for R(k,j)
    gradient = (-2 * Xij * Li[k]) + (2 * LiRj * Li[k]);
    Rj_update[k] = -gradient * step_size;
  }
  // Commit updates to Petuum PS
  for (int k = 0; k < FLAGS_K; ++k) {
    L_table.Inc(i,k,Li_update[k]);
    R_table.Inc(j,k,Rj_update[k]);
  }
}

// Initialize the Matrix Factorization solver
void init_mf(petuum::Table<float>& L_table, petuum::Table<float>& R_table) {
  // Create a uniform RNG in the range [0,1)
  boost::mt19937 generator(FLAGS_rng_seed);
  boost::uniform_real<> zero_to_one_dist(0,1);
  boost::variate_generator<boost::mt19937&,boost::uniform_real<> > zero_to_one_generator(generator, zero_to_one_dist);
  // Add a random initialization in [0,1)/num_workers to each element of L and R
  const int num_workers = get_total_num_workers();
  for (int i = 0; i < N_; ++i) {
    for (int k = 0; k < FLAGS_K; ++k) {
      double init_val = (2.0 * zero_to_one_generator() - 1.0) / num_workers;
      L_table.Inc(i,k,init_val);
    }
  }
  for (int j = 0; j < N_; ++j) {
    for (int k = 0; k < FLAGS_K; ++k) {
      double init_val = (2.0 * zero_to_one_generator() - 1.0) / num_workers;
      R_table.Inc(j,k,init_val);
    }
  }
}

// Outputs L_table and R_table to disk
void output_to_disk(petuum::Table<float>& L_table, petuum::Table<float>& R_table) {
  // L_table
  std::string L_file = FLAGS_output_prefix + ".L";
  std::ofstream L_stream(L_file.c_str());
  for (uint32_t i = 0; i < N_; ++i) {
    petuum::RowAccessor Li_acc;
    L_table.Get(i, &Li_acc);
    const petuum::DenseRow<float>& Li = Li_acc.Get<petuum::DenseRow<float> >();
    for (uint32_t k = 0; k < FLAGS_K; ++k) {
      L_stream << Li[k] << " ";
    }
    L_stream << "\n";
  }
  L_stream.close();
  // R_table
  std::string R_file = FLAGS_output_prefix + ".R";
  std::ofstream R_stream(R_file.c_str());
  for (uint32_t j = 0; j < M_; ++j) {
    petuum::RowAccessor Rj_acc;
    R_table.Get(j, &Rj_acc);
    const petuum::DenseRow<float>& Rj = Rj_acc.Get<petuum::DenseRow<float> >();
    for (uint32_t k = 0; k < FLAGS_K; ++k) {
      R_stream << Rj[k] << " ";
    }
    R_stream << "\n";
  }
  R_stream.close();
}

// Main Matrix Factorization routine, called by pthread_create
void* solve_mf(void* args) {
  int local_thread_id = *reinterpret_cast<int*>(args);
  // Register this thread with Petuum PS
  petuum::TableGroup::RegisterThread();
  // Get tables
  petuum::Table<float> L_table = petuum::TableGroup::GetTableOrDie<float>(0);
  petuum::Table<float> R_table = petuum::TableGroup::GetTableOrDie<float>(1);
  petuum::Table<float> loss_table = petuum::TableGroup::GetTableOrDie<float>(2);
  // Initialize MF solver
  const int total_num_workers = get_total_num_workers();
  const int global_worker_id = get_global_worker_id(local_thread_id); // global_worker_id lies in the range [0,total_num_workers)
  init_mf(L_table,R_table);
  // Run additional iterations to let stale values finish propagating
  for (int iter = 0; iter < FLAGS_staleness; ++iter) {
    petuum::TableGroup::Clock();
  }
  // Run MF solver
  for (int iter = 0; iter < FLAGS_num_iterations; ++iter) {
    boost::posix_time::ptime beginT = boost::posix_time::microsec_clock::local_time();
    if (global_worker_id == 0) {
      std::cout << "Iteration " << iter+1 << "/" << FLAGS_num_iterations << "... " << std::flush;
    }
    // Clear loss function table
    petuum::RowAccessor loss_acc;
    loss_table.Get(0, &loss_acc);
    const petuum::DenseRow<float>& loss_row = loss_acc.Get<petuum::DenseRow<float> >();
    loss_table.Inc(0,global_worker_id,-loss_row[global_worker_id]);
    // Divide matrix elements across workers, and perform SGD
    for (int a = global_worker_id; a < X_row.size(); a += total_num_workers) {
      sgd_element(a,iter,global_worker_id,L_table,R_table,loss_table);
    }
    // Output loss function
    if (global_worker_id == 0) {
      petuum::RowAccessor loss_acc;
      loss_table.Get(0, &loss_acc);
      const petuum::DenseRow<float>& loss_row = loss_acc.Get<petuum::DenseRow<float> >();
      float loss = 0.0;
      for (int t = 0; t < total_num_workers; ++t) {
        loss += loss_row[t];
      }
      std::cout << "loss function = " << loss << "... " << std::flush;
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
    output_to_disk(L_table,R_table);
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
  table_group_config.num_tables = 3;  // L_table, R_table, loss_table
  // Local params
  table_group_config.num_local_server_threads = 1;
  table_group_config.num_local_bg_threads = 1;
  table_group_config.num_local_app_threads = FLAGS_num_worker_threads + 1;  // +1 for main() thread
  petuum::GetHostInfos(FLAGS_hostfile, &table_group_config.host_map);
  petuum::GetServerIDsFromHostMap(&table_group_config.server_ids, table_group_config.host_map);
  table_group_config.client_id = FLAGS_client_id;
  table_group_config.consistency_model = petuum::SSPPush;
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
  read_sparse_matrix(FLAGS_datafile);
  if (FLAGS_client_id == 0) {
    std::cout << "done" << std::endl;
    // Output statistics
    std::cout << "Matrix dimensions: " << N_ << " by " << M_ << std::endl;
    std::cout << "# non-missing entries: " << X_row.size() << std::endl;
    std::cout << "Factorization rank: " << FLAGS_K << std::endl;
    std::cout << "# client machines: " << FLAGS_num_clients << std::endl;
    std::cout << "# worker threads per client: " << FLAGS_num_worker_threads << std::endl;
    std::cout << "SSP staleness: " << FLAGS_staleness << std::endl;
  }
  
  // Configure PS tables
  petuum::ClientTableConfig table_config;
  table_config.table_info.row_type = 0; // Dense rows
  table_config.oplog_capacity = 100;
  // L_table (N by K)
  table_config.table_info.table_staleness = FLAGS_staleness;
  table_config.table_info.row_capacity = FLAGS_K;
  table_config.process_cache_capacity = N_;
  petuum::TableGroup::CreateTable(0,table_config);
  // R_table (M by K)
  table_config.table_info.table_staleness = FLAGS_staleness;
  table_config.table_info.row_capacity = FLAGS_K;
  table_config.process_cache_capacity = M_;
  petuum::TableGroup::CreateTable(1,table_config);
  // loss_table (1 by total # workers)
  table_config.table_info.table_staleness = 0;  // No staleness for loss table
  table_config.table_info.row_capacity = get_total_num_workers();
  table_config.process_cache_capacity = 1;
  petuum::TableGroup::CreateTable(2,table_config);
  // Finished creating tables
  petuum::TableGroup::CreateTableDone();
  
  // Run Petuum PS-based MF solver
  pthread_t* threads = new pthread_t[FLAGS_num_worker_threads];
  int* local_thread_ids = new int[FLAGS_num_worker_threads];
  for (int t = 0; t < FLAGS_num_worker_threads; ++t) {
    local_thread_ids[t] = t;
    pthread_create(threads+t, NULL, solve_mf, local_thread_ids+t);
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
