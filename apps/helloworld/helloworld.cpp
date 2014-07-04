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
 * author: jinlianw
 */

#include <petuum_ps/include/petuum_ps.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <pthread.h>

DEFINE_string(hostfile, "", "Path to file containing server ip:port.");
DEFINE_int32(num_total_server_threads, 1, "Total number of server threads");
DEFINE_int32(num_total_bg_threads, 1, "Total number of background threads");
DEFINE_int32(num_total_clients, 1, "Total number of clients");
DEFINE_int32(num_tables, 1, "Total number of tables");
DEFINE_int32(num_server_threads, 2, "Number of server threads in this client");
DEFINE_int32(num_app_threads, 2, "Number of app threads in this client");
DEFINE_int32(num_bg_threads, 1, "Number of bg threads on this client");
DEFINE_int32(client_id, 0, "Client ID");

DEFINE_int32(num_iterations, 0, "Number iterations");
DEFINE_int32(staleness, 0, "Staleness");

DEFINE_int32(snapshot_clock, 10000, "How frequent snapshot is taken");
DEFINE_int32(resume_clock, 0, "From which clock should I start");
DEFINE_string(snapshot_dir, "/tmp", "Snapshot directory");

struct ThreadContext {
  int32_t num_clients;
  int32_t client_id;
  int32_t app_thread_id_offset;
  int32_t num_app_threads_per_client;
  int32_t num_iterations;
  int32_t staleness;
  int32_t num_tables;
  int32_t resume_clock;
};

void StalenessTest(const ThreadContext &thread_context, int32_t thread_id) {
  int32_t iteration_num;
  VLOG(0) << "num_tables = " << thread_context.num_tables;
  petuum::Table<int> *tables
    = new petuum::Table<int>[thread_context.num_tables];
  int32_t table_id = 0;
  for (table_id = 0; table_id < thread_context.num_tables; ++table_id) {
    tables[table_id] = petuum::TableGroup::GetTableOrDie<int>(table_id);
    CHECK_EQ(tables[table_id].get_row_type(), 20);
    VLOG(0) << "row_type = " << tables[table_id].get_row_type();
  }

  for (iteration_num = 0; iteration_num < thread_context.num_iterations;
       ++iteration_num) {
    // Test 1: read and verify my cell is valid
    for (table_id = 0; table_id < thread_context.num_tables; ++table_id) {
      petuum::RowAccessor row_acc_0, row_acc_mine;

      tables[table_id].Get(0, &row_acc_0);
      int val = (row_acc_0.Get<petuum::DenseRow<int> >())[thread_id];
      CHECK_EQ(val, iteration_num + thread_context.resume_clock)
          << "thread_id = " << thread_id;

      tables[table_id].Get(thread_id, &row_acc_mine);
      val = (row_acc_mine.Get<petuum::DenseRow<int> >())[thread_id];
      CHECK_EQ(val, iteration_num + thread_context.resume_clock);
    }
    // Update my cell
    petuum::UpdateBatch<int> update_batch;
    update_batch.Update(thread_id, 1);
    for (table_id = 0; table_id < thread_context.num_tables; ++table_id) {
      tables[table_id].BatchInc(0, update_batch);
      VLOG(0) << "thread " << thread_id << " iteration " << iteration_num
	      << " updated row " << 0 << " column_id " << thread_id;
      tables[table_id].BatchInc(thread_id, update_batch);
      VLOG(0) << "thread " << thread_id << " iteration " << iteration_num
	      << " updated row " << thread_id << " column_id " << thread_id;
    }

    VLOG(0) << "Calling clock from thread " << thread_id;
    petuum::TableGroup::Clock();
  }
  delete[] tables;
}

void *WorkerThreadMain(void *argu) {
  ThreadContext *thread_context = reinterpret_cast<ThreadContext*>(argu);
  int32_t thread_id = petuum::TableGroup::RegisterThread();
  VLOG(0) << "Thread has registered, thread_id = " << thread_id;

  StalenessTest(*thread_context, thread_id);

  petuum::TableGroup::DeregisterThread();
  return 0;
}

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  petuum::TableGroupConfig table_group_config;
  table_group_config.num_total_server_threads = FLAGS_num_total_server_threads;
  table_group_config.num_total_bg_threads = FLAGS_num_total_bg_threads;
  table_group_config.num_total_clients = FLAGS_num_total_clients;

  table_group_config.num_tables = FLAGS_num_tables;

  table_group_config.num_local_server_threads = FLAGS_num_server_threads;
  table_group_config.num_local_app_threads = FLAGS_num_app_threads;
  table_group_config.num_local_bg_threads = FLAGS_num_bg_threads;

  //VLOG(0) << "hostfile: " << FLAGS_hostfile;
  petuum::GetHostInfos(FLAGS_hostfile, &table_group_config.host_map);

  petuum::GetServerIDsFromHostMap(&(table_group_config.server_ids),
    table_group_config.host_map);

  table_group_config.client_id = 0;
  table_group_config.consistency_model = petuum::SSP;

  table_group_config.snapshot_clock = FLAGS_snapshot_clock;
  table_group_config.resume_clock = FLAGS_resume_clock;
  table_group_config.snapshot_dir = FLAGS_snapshot_dir;

  VLOG(0) << "Helloworld starts here";

  petuum::TableGroup::RegisterRow<petuum::DenseRow<int> >(20);

  int32_t init_thread_id = petuum::TableGroup::Init(table_group_config, true);

  VLOG(0) << "Initialized TableGroup, thread id = " << init_thread_id;

  petuum::ClientTableConfig table_config;
  table_config.table_info.table_staleness = FLAGS_staleness;
  table_config.table_info.row_type = 20;
  table_config.table_info.row_capacity = 1000;

  table_config.process_cache_capacity = 100;
  table_config.thread_cache_capacity = 100;
  table_config.oplog_capacity = 100;

  int32_t table_id = 0;
  for (table_id = 0; table_id < FLAGS_num_tables; ++table_id) {
    VLOG(0) << "Create table_id = " << table_id;
    bool suc = petuum::TableGroup::CreateTable(table_id, table_config);
    CHECK(suc);
  }

  petuum::TableGroup::CreateTableDone();

  int32_t num_worker_threads = FLAGS_num_app_threads - 1;
  pthread_t *threads = 0;

  ThreadContext thread_context;
  thread_context.num_clients = FLAGS_num_total_clients;
  thread_context.client_id = FLAGS_client_id;
  // assuming all clients have the same number of app threads
  thread_context.num_app_threads_per_client = FLAGS_num_app_threads;
  thread_context.app_thread_id_offset = FLAGS_num_server_threads
    + FLAGS_num_bg_threads + 1;
  thread_context.num_iterations = FLAGS_num_iterations;
  thread_context.staleness = FLAGS_staleness;
  thread_context.num_tables = FLAGS_num_tables;
  thread_context.resume_clock = FLAGS_resume_clock;

  if (num_worker_threads > 0) {
    threads = new pthread_t[num_worker_threads];
    for (int i = 0; i < num_worker_threads; ++i) {
      pthread_create(threads + i, NULL, WorkerThreadMain, &thread_context);
    }
  }

  petuum::TableGroup::WaitThreadRegister();
  StalenessTest(thread_context, init_thread_id);

  if (num_worker_threads > 0) {
    for (int i = 0; i < num_worker_threads; ++i) {
      pthread_join(threads[i], NULL);
    }
  }

  delete[] threads;
  petuum::TableGroup::ShutDown();
  return 0;
}
