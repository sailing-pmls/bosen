// Copyright (c) 2013, SailingLab
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
 *  Author: wdai, jinlianw
 */

#include <petuum_ps/util/utils.hpp>
#include <petuum_ps/include/petuum_ps.hpp>
#include <zmq.hpp>
#include <pthread.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <iostream>
#include <pthread.h>
#include <assert.h>

DEFINE_int32(myid, 0, "Client ID uniquely identifies a client machine.");
DEFINE_string(serverfile, "",
    "Path to file containing server ip:port.");
DEFINE_int32(nthrs, 1, "number of threads");
DEFINE_string(config_file, "",
    "Path to .cfg file containing client configs.");

struct ThreadInfo{
  petuum::TableGroup<float> &table_group;
  petuum::DenseTable<float> &table;

  ThreadInfo(petuum::TableGroup<float> &_table_group, petuum::DenseTable<float> &_table):
    table_group(_table_group), table(_table){}
};

void *thread_main(void *thrinfo);

int main(int argc, char *argv[]) {

  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  LOG(INFO) << "simple app started with myid: " << FLAGS_myid
    << " serverfile: " << FLAGS_serverfile
    << " num threads: " << FLAGS_nthrs;

  // Read in server configuration.
  std::vector<petuum::ServerInfo> servers
    = petuum::GetServerInfos(FLAGS_serverfile);

  // Read in client config which contains config for each table.
  std::map<int, petuum::TableConfig> table_configs_map =
    petuum::ReadTableConfigs(FLAGS_config_file);

  // Create table_group.
  zmq::context_t *zmq_ctx = new zmq::context_t(1);
  petuum::TableGroupConfig table_group_config(FLAGS_myid,
      FLAGS_nthrs, servers, zmq_ctx);

  petuum::TableGroup<float>& table_group =
    petuum::TableGroup<float>::CreateTableGroup(table_group_config);

  // all threads need to register with table group.
  int suc = table_group.RegisterThread();
  assert(suc == 0);

  // Create a dense_table.
  int32_t table_id = 1;
  LOG(INFO) << "Creating table " << table_id;
  LOG_IF(FATAL, table_configs_map.count(table_id) == 0)
    << "Table (table_id = " << table_id << ") is not in the config file "
    << FLAGS_config_file;
  petuum::DenseTable<float>& dense_table =
    table_group.CreateDenseTable(table_id, table_configs_map[table_id]);

  table_id = 2;
  LOG(INFO) << "Creating table " << table_id;
  LOG_IF(FATAL, table_configs_map.count(table_id) == 0)
    << "Table (table_id = " << table_id << ") is not in the config file "
    << FLAGS_config_file;
  petuum::DenseTable<float>& dense_table2 =
    table_group.CreateDenseTable(table_id, table_configs_map[table_id]);

  ThreadInfo thrinfo(table_group, dense_table);

  pthread_t *thr = new pthread_t[FLAGS_nthrs];

  std::cout << "\n\nCreate Client Threads" << std::endl;

  for(int i = 0; i < FLAGS_nthrs; ++i){
    suc = pthread_create(thr + i, NULL, 
        thread_main,
        &thrinfo);
    assert(suc == 0);
  }

  for(int i = 0; i < FLAGS_nthrs; ++i){
    pthread_join(thr[i], NULL);
  }
  LOG(INFO) << "pthread_join returned";
  delete[] thr;

  // then deregister threads
  suc = table_group.DeregisterThread();
  assert(suc == 0);

  table_group.ShutDown();

  delete zmq_ctx;

  return 0;
}

void *thread_main(void *info){

  ThreadInfo *thrinfo = (ThreadInfo *) info; 
  petuum::TableGroup<float> &table_group = thrinfo->table_group;
  petuum::DenseTable<float> &table = thrinfo->table;

  // The first thing you do is register the thread
  int suc = table_group.RegisterThread();
  assert(suc == 0);
  table_group.RegisterExecutionThread();

  //========= Iteration 0 STARTS ==================
  LOG(INFO) << "========== Iteration 0 starts ==============";

  table.Inc(1, 1, 2.2);
  table.Inc(2, 3, 1);

  float a = table.Get(1, 1);

  a = table.Get(2, 3);

  table_group.Iterate();
  LOG(INFO) << "========== Iteration 0 ends ==============";
  //========= Iteration 0 END ==================

  //========= Iteration 1 STARTS ==================
  LOG(INFO) << "========== Iteration 1 starts ==============";

  a = table.Get(1, 1);
  std::cout << "table(1, 1) = " << a << std::endl;

  table.Inc(5, 7, 10);

  a = table.Get(3, 4);
  assert(a == 0);

  table_group.Iterate();
  LOG(INFO) << "========== Iteration 1 ends ==============";
  //========= Iteration 1 END ==================

  LOG(INFO) << "========== Iteration 2 starts ==============";
  table.Inc(5, 7, -2);
  table_group.Iterate();
  LOG(INFO) << "========== Iteration 2 ends ==============";

  a = table.Get(5, 7);
  std::cout << "table(5, 7) = " << a << std::endl;

  // the last thing you do is deregister the thread
  suc = table_group.DeregisterThread();
  assert(suc == 0);

  return NULL;
}
