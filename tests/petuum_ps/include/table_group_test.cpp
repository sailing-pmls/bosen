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
#include "petuum_ps/include/table_group.hpp"
#include "petuum_ps/include/table.hpp"

#include <vector>
#include <boost/scoped_ptr.hpp>
#include <gtest/gtest.h>

namespace petuum {

const int kNumColumns = 100;
const int kProcessStorageCapacity = 3;
const int kThreadCacheCapacity = 3;
const int kMaxPendingOpLogs = 5;

zmq::context_t zmq_ctx(1);

class TableGroupTest : public testing::Test {
  protected:
    TableGroupTest() {
      ServerInfo ser(0, "127.0.0.1", "9999");
      std::vector<ServerInfo> ser_vec;
      ser_vec.push_back(ser);
      TableGroupConfig conf(23, ser_vec, &zmq_ctx);
      tg = &(TableGroup<int>::CreateTableGroup(conf));

      // StorageConfig for process cache
      StorageConfig process_storage_config;
      process_storage_config.max_size = kProcessStorageCapacity;

      // OpLogManagerConfig
      OpLogManagerConfig op_log_config;
      op_log_config.thread_cache_capacity = kThreadCacheCapacity;
      op_log_config.max_pending_op_logs = kMaxPendingOpLogs;

      // General table config.
      table_config_.table_staleness = 0;
      table_config_.num_columns = kNumColumns;
      // Ignore server table, which is handled by TableGroup.
      table_config_.process_storage_config = process_storage_config;
      table_config_.op_log_config = op_log_config;
    }

    TableGroup<int> *tg;
    TableConfig table_config_;
};

TEST_F(TableGroupTest, ConstructorTest) {
}

TEST_F(TableGroupTest, CreateTableTest) {
  tg->RegisterThread();
  DenseTable<int>& dt = tg->CreateDenseTable(123, table_config_);
  tg->DeregisterThread();
}





}
