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
#include "petuum_ps/include/table.hpp"
#include "tests/petuum_ps/proxy/dummy_client_proxy.hpp"
#include <boost/scoped_ptr.hpp>
#include <gtest/gtest.h>

namespace petuum {

namespace {

const int kNumColumns = 100;
const int kProcessStorageCapacity = 3;
const int kThreadCacheCapacity = 3;
const int kMaxPendingOpLogs = 5;

}  // anonymous namespace

class TableTest : public testing::Test {
  protected:
    TableTest() {

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

      table_id_ = 1;
    }

    // This is proxy method to access private constructor in Table.
    DenseTable<int>* ConstructDenseTable(int32_t table_id,
        const TableConfig& table_config, ClientProxy<int>& client_proxy) {
      return new DenseTable<int>(table_id, table_config, client_proxy);
    }

    // Proxy method for private Iterate call.
    void IterateProxy(DenseTable<int>* dense_table) {
      dense_table->Iterate();
    }

    TableConfig table_config_;

    DummyClientProxy<int> dummy_proxy_;

    int32_t table_id_;
};

TEST_F(TableTest, Constructors) {
  boost::scoped_ptr<DenseTable<int> > dense_table;
  dense_table.reset(ConstructDenseTable(table_id_, table_config_,
        dummy_proxy_));
}

TEST_F(TableTest, Get) {
  boost::scoped_ptr<DenseTable<int> > dense_table;
  dense_table.reset(ConstructDenseTable(table_id_, table_config_,
        dummy_proxy_));
  // Dummy client always returns (1, 2, 3) from server.
  EXPECT_EQ(1, dense_table->Get(0, 0));
  EXPECT_EQ(2, dense_table->Get(1, 1));
}


TEST_F(TableTest, Put) {
  boost::scoped_ptr<DenseTable<int> > dense_table;
  dense_table.reset(ConstructDenseTable(table_id_, table_config_,
        dummy_proxy_));
  // Dummy client always returns (1, 2, 3) from server.
  dense_table->Put(0, 0, 99);
  EXPECT_EQ(2, dense_table->Get(1, 1));
  EXPECT_EQ(99, dense_table->Get(0, 0));
}

TEST_F(TableTest, Inc) {
  boost::scoped_ptr<DenseTable<int> > dense_table;
  dense_table.reset(ConstructDenseTable(table_id_, table_config_,
        dummy_proxy_));
  // Dummy client always returns (1, 2, 3) from server.
  dense_table->Inc(0, 0, 99);
  EXPECT_EQ(2, dense_table->Get(1, 1));
  EXPECT_EQ(100, dense_table->Get(0, 0));
}

TEST_F(TableTest, Iterate) {
  boost::scoped_ptr<DenseTable<int> > dense_table;
  dense_table.reset(ConstructDenseTable(table_id_, table_config_,
        dummy_proxy_));
  // Dummy client always returns (1, 2, 3) from server.
  dense_table->Inc(0, 0, 99);
  EXPECT_EQ(100, dense_table->Get(0, 0));
  IterateProxy(dense_table.get());
}

}  // namespace petuum
