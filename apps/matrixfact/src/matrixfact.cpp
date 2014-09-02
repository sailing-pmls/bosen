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
#include <cstdint>
#include <random>

#include <petuum_ps_common/include/petuum_ps.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <pthread.h>

#include <boost/date_time/posix_time/posix_time.hpp>

#include "matrixloader.hpp"

// Command-line flags
DEFINE_string(hostfile, "", "Path to Petuum PS server configuration file");
DEFINE_int32(ps_row_in_memory_limit, 1000, "Single-machine version only: max # rows of L, R that can be held in memory");
DEFINE_int32(ps_row_cache_size, 0, "Max # rows of L, R that can be cached per client; set to 0 (default) to cache all rows. If you are running out of memory due to big L, R, consider limiting the cache size. No effect for single-machine version.");
DEFINE_string(datafile, "", "Input sparse matrix");
DEFINE_string(offsetsfile, "", "Offsets file for datafile - if provided, the program will run in disk-based mode to save memory");
DEFINE_string(output_prefix, "", "Output results with this prefix. If the prefix is X, the output will be called X.L and X.R");
DEFINE_double(lambda, 0.0, "L2 regularization strength; 0 disables");
DEFINE_double(init_step_size, 0.5, "SGD step size at iteration t is init_step_size * (step_size_offset + t)^(-step_size_pow)");
DEFINE_double(step_size_offset, 100.0, "SGD step size at iteration t is init_step_size * (step_size_offset + t)^(-step_size_pow)");
DEFINE_double(step_size_pow, 0.5, "SGD step size at iteration t is init_step_size * (step_size_offset + t)^(-step_size_pow)");
DEFINE_int32(rng_seed, 967234, "RNG seed for initializing factor matrices L, R");
DEFINE_int32(num_clients, 1, "Total number of clients");
DEFINE_int32(num_worker_threads, 1, "Number of worker threads per client");
DEFINE_int32(client_id, 0, "This client's ID");
DEFINE_int32(K, 100, "Factorization rank");
DEFINE_int32(num_iterations, 100, "Number of iterations");
DEFINE_int32(staleness, 0, "Staleness");
DEFINE_string(ps_stats_path, "", "Petuum PS statistics output file");
DEFINE_int32(ps_snapshot_clock, 0, "How often to take PS snapshots; 0 disables");
DEFINE_int32(ps_resume_clock, 0, "If nonzero, resume from the PS snapshot with this ID");
DEFINE_string(ps_snapshot_dir, "", "Where to save PS snapshots");
DEFINE_string(ps_resume_dir, "", "Where to retrieve PS snapshots for resuming");

// Typedefs
typedef std::pair<int,float> int_pair_float_t;

// Data variables
matrixloader::AbstractMatrixLoader* data_matrix;

// Returns the number of workers threads across all clients
int get_total_num_workers() {
  return FLAGS_num_clients * FLAGS_num_worker_threads;
}
// Returns the global thread ID of this worker
int get_global_worker_id(int local_thread_id) {
  return (FLAGS_client_id * FLAGS_num_worker_threads) + local_thread_id;
}

// Performs stochastic gradient descent on the (i,j)-th element of the matrix,
// which takes value Xij.
void sgd_element(const int i, const int j, const float Xij, float step_size, int global_worker_id,
                 petuum::Table<float>& L_table,
                 petuum::Table<float>& R_table,
                 petuum::Table<float>& loss_table) {
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
  // Update the loss function (does not include L2 regularizer term)
  loss_table.Inc(0,global_worker_id,pow(Xij - LiRj, 2));
  
  // Now update L(i,:) and R(:,j) based on the loss function at X(i,j).
  // The non-regularized loss function at X(i,j) is ( X(i,j) - L(i,:)*R(:,j) )^2.
  //
  // The non-regularized gradient w.r.t. L(i,k) is -2*X(i,j)R(k,j) + 2*L(i,:)*R(:,j)*R(k,j).
  // The non-regularized gradient w.r.t. R(k,j) is -2*X(i,j)L(i,k) + 2*L(i,:)*R(:,j)*L(i,k).
  petuum::UpdateBatch<float> Li_update;
  petuum::UpdateBatch<float> Rj_update;
  for (int k = 0; k < FLAGS_K; ++k) {
    float gradient = 0.0;
    // Compute update for L(i,k)
    gradient = -2 * (Xij - LiRj) * Rj[k] + FLAGS_lambda * 2 * Li[k];
    Li_update.Update(k, -gradient * step_size);
    // Compute update for R(k,j)
    gradient = -2 * (Xij - LiRj) * Li[k] + FLAGS_lambda * 2 * Rj[k];
    Rj_update.Update(k, -gradient * step_size);
  }
  // Commit updates to Petuum PS
  L_table.BatchInc(i,Li_update);
  R_table.BatchInc(j,Rj_update);
}

// Initialize the Matrix Factorization solver
void init_mf(petuum::Table<float>& L_table, petuum::Table<float>& R_table) {
  // Uniform RNG that produces numbers in [-1,1)
  std::mt19937 generator(FLAGS_rng_seed);
  std::uniform_real_distribution<> dist(-1.,1.);
  // Add a random initialization in [-1,1)/num_workers to each element of L and R
  const int num_workers = get_total_num_workers();
  for (int i = 0; i < data_matrix->getN(); ++i) {
    petuum::UpdateBatch<float> L_updates;
    for (int k = 0; k < FLAGS_K; ++k) {
      double init_val = dist(generator) / num_workers;
      L_updates.Update(k,init_val);
    }
    L_table.BatchInc(i,L_updates);
  }
  for (int j = 0; j < data_matrix->getM(); ++j) {
    petuum::UpdateBatch<float> R_updates;
    for (int k = 0; k < FLAGS_K; ++k) {
      double init_val = dist(generator) / num_workers;
      R_updates.Update(k,init_val);
    }
    R_table.BatchInc(j,R_updates);
  }
}

// Outputs L_table and R_table to disk
void output_to_disk(petuum::Table<float>& L_table, petuum::Table<float>& R_table) {
  // L_table
  std::string L_file = FLAGS_output_prefix + ".L";
  std::ofstream L_stream(L_file.c_str());
  for (uint32_t i = 0; i < data_matrix->getN(); ++i) {
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
  for (uint32_t j = 0; j < data_matrix->getM(); ++j) {
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
  petuum::PSTableGroup::RegisterThread();
  // Get tables
  petuum::Table<float> L_table = petuum::PSTableGroup::GetTableOrDie<float>(0);
  petuum::Table<float> R_table = petuum::PSTableGroup::GetTableOrDie<float>(1);
  petuum::Table<float> loss_table = petuum::PSTableGroup::GetTableOrDie<float>(2);
  // Initialize MF solver
  const int total_num_workers = get_total_num_workers();
  const int global_worker_id = get_global_worker_id(local_thread_id); // global_worker_id lies in the range [0,total_num_workers)
  STATS_APP_INIT_BEGIN();
  if (FLAGS_ps_resume_clock == 0) { // Only initialize L, R tables if not resuming
    init_mf(L_table,R_table);
  } else if (global_worker_id == 0) {
    std::cout << "...Resuming at Iteration " << FLAGS_ps_resume_clock-FLAGS_staleness
              << " (Snapshot " << FLAGS_ps_resume_clock << ")" << std::endl;
  }
  petuum::PSTableGroup::GlobalBarrier();  // Let stale values finish propagating (performs staleness+1 clock()s)
  STATS_APP_INIT_END();
  // Run MF solver
  for (int iter = 0; iter < FLAGS_num_iterations; ++iter) {
    // If resuming, skip iterations until we reach the correct iteration
    if (iter < FLAGS_ps_resume_clock-FLAGS_staleness-1) {
      petuum::PSTableGroup::Clock();
      continue;
    }
    
    // Begin iteration
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
    bool last_el = false;
    int64_t row,col;
    float val;
    STATS_APP_ACCUM_COMP_BEGIN();
    float step_size = FLAGS_init_step_size * pow(FLAGS_step_size_offset + iter, -FLAGS_step_size_pow);
    while (last_el == false) {
      data_matrix->getNextEl(global_worker_id,row,col,val,last_el);
      sgd_element(row,col,val,step_size,global_worker_id,L_table,R_table,loss_table);
    }
    STATS_APP_ACCUM_COMP_END();
    
    // Output loss function (does not include L2 regularizer term)
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
    petuum::PSTableGroup::Clock();
    boost::posix_time::time_duration elapTime = boost::posix_time::microsec_clock::local_time() - beginT;
    if (global_worker_id == 0) {
      std::cout << "elapsed time = " << ((float) elapTime.total_milliseconds()) / 1000 << std::endl;
    }
  }
  // Let stale values finish propagating (performs staleness+1 clock()s)
  petuum::PSTableGroup::GlobalBarrier();
  // Output results to disk
  if (global_worker_id == 0) {
    std::cout << "Outputting results to prefix " << FLAGS_output_prefix << " ... " << std::flush;
    output_to_disk(L_table,R_table);
    std::cout << "done" << std::endl;
  }
  // Deregister this thread with Petuum PS
  petuum::PSTableGroup::DeregisterThread();
  return 0;
}

// Main function
int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  boost::posix_time::ptime beginT = boost::posix_time::microsec_clock::local_time();
  
  // Configure Petuum PS
  petuum::TableGroupConfig table_group_config;
  // Global parameters for the whole PS system
  table_group_config.num_total_server_threads = FLAGS_num_clients;  // 1 server thread per client
  table_group_config.num_total_bg_threads = FLAGS_num_clients;  // 1 background thread per client
  table_group_config.num_total_clients = FLAGS_num_clients;
  table_group_config.num_tables = 3;  // L_table, R_table, loss_table
  // Single-node-PS versus multi-machine PS
#ifdef PETUUM_SINGLE_NODE
  table_group_config.ooc_path_prefix = FLAGS_output_prefix + ".matrixfact_sn.localooc";
  table_group_config.consistency_model = petuum::LocalOOC;
#else
  petuum::GetHostInfos(FLAGS_hostfile, &table_group_config.host_map);
  petuum::GetServerIDsFromHostMap(&table_group_config.server_ids, table_group_config.host_map);
  table_group_config.consistency_model = petuum::SSPPush;
#endif
  // Local parameters for this process
  table_group_config.num_local_server_threads = 1;
  table_group_config.num_local_bg_threads = 1;
  table_group_config.num_local_app_threads = FLAGS_num_worker_threads + 1;  // +1 for main() thread
  table_group_config.client_id = FLAGS_client_id;
  // Stats and snapshots
  table_group_config.stats_path = FLAGS_ps_stats_path;
  table_group_config.snapshot_clock = FLAGS_ps_snapshot_clock;  // Only for multi-machine PS
  table_group_config.resume_clock = FLAGS_ps_resume_clock;  // Only for multi-machine PS
  table_group_config.snapshot_dir = FLAGS_ps_snapshot_dir;  // Only for multi-machine PS
  table_group_config.resume_dir = FLAGS_ps_resume_dir;  // Only for multi-machine PS
  // Configure PS row types
  petuum::PSTableGroup::RegisterRow<petuum::DenseRow<float> >(0);  // Register dense rows as ID 0
  // Start PS
  // IMPORTANT: This command starts up the name node service on client 0.
  //            We therefore do it ASAP, before other lengthy actions like
  //            loading data.
  petuum::PSTableGroup::Init(table_group_config, false);  // Initializing thread does not need table access
  
  // Load Data
  STATS_APP_LOAD_DATA_BEGIN();
  if (FLAGS_offsetsfile.size() == 0) { // Load the full matrix
    if (FLAGS_client_id == 0) {
      std::cout << "Data mode: Loading matrix " << FLAGS_datafile << " into memory..." << std::endl;
    }
    data_matrix = new matrixloader::StandardMatrixLoader(FLAGS_datafile,get_total_num_workers());
  } else { // Disk-based loader
    if (FLAGS_client_id == 0) {
      std::cout << "Data mode: Streaming matrix " << FLAGS_datafile << " from disk using offsets " << FLAGS_offsetsfile << std::endl;
    }
    auto dml = new matrixloader::DiskMatrixLoader(FLAGS_datafile,FLAGS_offsetsfile,get_total_num_workers());
    for (int t = 0; t < FLAGS_num_worker_threads; ++t) {  // Initialize workers
      dml->initWorker(get_global_worker_id(t));
    }
    data_matrix = dml;
  }
  STATS_APP_LOAD_DATA_END();
  
  // Output statistics
  if (FLAGS_client_id == 0) {
    std::cout << "Matrix dimensions: " << data_matrix->getN() << " by " << data_matrix->getM() << std::endl;
    std::cout << "# non-missing entries: " << data_matrix->getNNZ() << std::endl;
    std::cout << "Factorization rank: " << FLAGS_K << std::endl;
    std::cout << "# client machines: " << FLAGS_num_clients << std::endl;
    std::cout << "# worker threads per client: " << FLAGS_num_worker_threads << std::endl;
    std::cout << "SSP staleness: " << FLAGS_staleness << std::endl;
    std::cout << "Step size formula: " << FLAGS_init_step_size << " * (" << FLAGS_step_size_offset << " + t)^(-" << FLAGS_step_size_pow << ")" << std::endl;
    std::cout << "Regularization strength lambda: " << FLAGS_lambda << std::endl;
    std::cout << "  (Note: displayed loss function does not include regularization term)" << std::endl;
  }
  
  // Configure PS tables
  petuum::ClientTableConfig table_config;
  table_config.table_info.row_type = 0; // Dense rows
  table_config.oplog_capacity = 100;
  // L_table (N by K)
  table_config.table_info.table_staleness = FLAGS_staleness;
  table_config.table_info.row_capacity = FLAGS_K;
#ifdef PETUUM_SINGLE_NODE
  table_config.process_cache_capacity = FLAGS_ps_row_in_memory_limit;
#else
  if (FLAGS_ps_row_cache_size == 0) {
    table_config.process_cache_capacity = data_matrix->getN();
  } else {
    table_config.process_cache_capacity = FLAGS_ps_row_cache_size;
  }
#endif
  petuum::PSTableGroup::CreateTable(0,table_config);
  // R_table (M by K)
  table_config.table_info.table_staleness = FLAGS_staleness;
  table_config.table_info.row_capacity = FLAGS_K;
#ifdef PETUUM_SINGLE_NODE
  table_config.process_cache_capacity = FLAGS_ps_row_in_memory_limit;
#else
  if (FLAGS_ps_row_cache_size == 0) {
    table_config.process_cache_capacity = data_matrix->getM();
  } else {
    table_config.process_cache_capacity = FLAGS_ps_row_cache_size;
  }
#endif
  petuum::PSTableGroup::CreateTable(1,table_config);
  // loss_table (1 by total # workers)
  table_config.table_info.table_staleness = 0;  // No staleness for loss table
  table_config.table_info.row_capacity = get_total_num_workers();
  table_config.process_cache_capacity = 1;
  petuum::PSTableGroup::CreateTable(2,table_config);
  // Finished creating tables
  petuum::PSTableGroup::CreateTableDone();
  
  // Run Petuum PS-based MF solver
  pthread_t* threads = new pthread_t[FLAGS_num_worker_threads];
  int* local_thread_ids = new int[FLAGS_num_worker_threads];
  for (int t = 0; t < FLAGS_num_worker_threads; ++t) {
    local_thread_ids[t] = t;
    pthread_create(threads+t, NULL, solve_mf, local_thread_ids+t);
  }
  petuum::PSTableGroup::WaitThreadRegister();
  for (int t = 0; t < FLAGS_num_worker_threads; ++t) {
    pthread_join(threads[t], NULL);
  }
  delete threads;
  delete local_thread_ids;
  delete data_matrix;
  
  // Cleanup and output runtime
  petuum::PSTableGroup::ShutDown();
  boost::posix_time::time_duration elapTime = boost::posix_time::microsec_clock::local_time() - beginT;
  if (FLAGS_client_id == 0) {
    std::cout << "total runtime = " << ((float) elapTime.total_milliseconds()) / 1000 << "s" << std::endl;
  }
  return 0;
}
