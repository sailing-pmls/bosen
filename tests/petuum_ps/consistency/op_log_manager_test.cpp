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

#include "petuum_ps/consistency/op_log_manager.hpp"
#include "petuum_ps/storage/dense_row.hpp"
#include "petuum_ps/include/configs.hpp"
#include <gtest/gtest.h>
// For vector creation.
#include <boost/assign/std/vector.hpp>
#include <vector>

namespace petuum {

namespace {

const int kThreadCacheSize = 3;
const int kMaxPendingOpLogs = 4;
const int kNumServers = 2;
const int kTableID = 1;

typedef typename EntryOp<int>::OpType OpType;

}  // anonymous namespace

class OpLogManagerTest : public testing::Test {
  protected:
    OpLogManagerTest() {
      op_log_config_.thread_cache_capacity = kThreadCacheSize;
      op_log_config_.max_pending_op_logs = kMaxPendingOpLogs;
      op_log_config_.num_servers = kNumServers;
      op_log_config_.table_id = kTableID;
    }

    virtual void SetUp() { }

    void InsertThreadCacheRowOf3(OpLogManager<DenseRow, int>* op_log_manager,
        int32_t row_id, int col1_val, int col2_val, int col3_val, int32_t
        row_iter = 0) {
      using namespace boost::assign; // bring 'operator+=()' into scope
      std::vector<int> row_data;
      row_data += col1_val, col2_val, col3_val;
      op_log_manager->InsertThreadCache(row_id, DenseRow<int>(row_data, row_iter));
    }

    bool VerifyEntryOp(const EntryOp<int>& entry_op, OpType type, int val) {
      return (entry_op.op == type) && (entry_op.val == val);
    }

    bool VerifyEntryOpExtended(const EntryOpExtended<int>& entry_op,
        int32_t row_id, int32_t col_id, OpType type, int val) {
      return (entry_op.op == type) && (entry_op.row_id == row_id)
        && (entry_op.col_id == col_id) && (entry_op.val == val);
    }

    OpLogManagerConfig op_log_config_;
    StatsObj stats_obj_;
};

TEST_F(OpLogManagerTest, Constructor) {
  OpLogManager<DenseRow, int> op_log_manager(op_log_config_, stats_obj_, kTableID);
  EXPECT_EQ(kMaxPendingOpLogs, op_log_manager.get_max_pending_op_logs());
}

TEST_F(OpLogManagerTest, GetNothing) {
  OpLogManager<DenseRow, int> op_log_manager(op_log_config_, stats_obj_, kTableID);
  int val;
  EXPECT_EQ(0, op_log_manager.Get(0, 0, &val));  // 0 for not found.
  DenseRow<int> row;
  EXPECT_EQ(0, op_log_manager.GetRow(0, &row));  // 0 for not found.
}


// Test coalescing Put + Inc
TEST_F(OpLogManagerTest, PutInc_0) {
  OpLogManager<DenseRow, int> op_log_manager(op_log_config_, stats_obj_, kTableID);
  EntryOp<int> entry_op;
  EXPECT_EQ(0, op_log_manager.GetOpLog(0, 1, &entry_op));  // 0 for not found.
  // Put and Inc and then check oplog.
  EXPECT_EQ(0, op_log_manager.Put(0, 1, 5));
  EXPECT_EQ(0, op_log_manager.Inc(0, 1, 6));
  // Op log for row 0 column 1 should be PUT 11.
  // 1 for found op log.
  EXPECT_EQ(1, op_log_manager.GetOpLog(0, 1, &entry_op));
  EXPECT_TRUE(VerifyEntryOp(entry_op, OpType::PUT, 11));
}

// Test coalescing Inc + Inc
TEST_F(OpLogManagerTest, PutInc_1) {
  OpLogManager<DenseRow, int> op_log_manager(op_log_config_, stats_obj_, kTableID);
  EntryOp<int> entry_op;
  EXPECT_EQ(0, op_log_manager.GetOpLog(0, 1, &entry_op));  // 0 for not found.
  // Put and Inc and then check oplog.
  EXPECT_EQ(0, op_log_manager.Inc(0, 1, 3));
  EXPECT_EQ(0, op_log_manager.Inc(0, 1, 4));
  // Op log for row 0 column 1 should be INC 7.
  // 1 for found op log.
  EXPECT_EQ(1, op_log_manager.GetOpLog(0, 1, &entry_op));
  EXPECT_TRUE(VerifyEntryOp(entry_op, OpType::INC, 7));
}

TEST_F(OpLogManagerTest, PutInc_2) {
  OpLogManager<DenseRow, int> op_log_manager(op_log_config_, stats_obj_, kTableID);
  EntryOp<int> entry_op;
  EXPECT_EQ(0, op_log_manager.GetOpLog(0, 1, &entry_op));  // 0 for not found.
  // Put and Inc and then check oplog.
  EXPECT_EQ(0, op_log_manager.Put(0, 1, 3));
  EXPECT_EQ(0, op_log_manager.Put(0, 1, 4));
  // Op log for row 0 column 1 should be PUT 4.
  // 1 for found op log.
  EXPECT_EQ(1, op_log_manager.GetOpLog(0, 1, &entry_op));
  EXPECT_TRUE(VerifyEntryOp(entry_op, OpType::PUT, 4));

  // Test ClearOpLogTable.
  op_log_manager.ClearOpLogTable();
  EXPECT_EQ(0, op_log_manager.GetOpLog(0, 1, &entry_op));  // 0 for not found.
}

// Test op_log_manager complains when there are too many pending op logs.
TEST_F(OpLogManagerTest, OpLogOutOfCapacity) {
  OpLogManager<DenseRow, int> op_log_manager(op_log_config_, stats_obj_, kTableID);
  // Put < kMaxPendingOpLogs op logs.
  EXPECT_EQ(0, op_log_manager.Put(0, 1, 3));
  EXPECT_EQ(0, op_log_manager.Put(0, 2, 3));
  EXPECT_EQ(0, op_log_manager.Put(0, 3, 3));

  // All of the followings should complain by returning 1
  EXPECT_EQ(1, op_log_manager.Inc(0, 4, 3));
  EXPECT_EQ(1, op_log_manager.Inc(0, 4, 3));
  EXPECT_EQ(1, op_log_manager.Put(0, 4, 3));
}

TEST_F(OpLogManagerTest, ThreadCache_0) {
  OpLogManager<DenseRow, int> op_log_manager(op_log_config_, stats_obj_, kTableID);
  InsertThreadCacheRowOf3(&op_log_manager, 0, 11, 22, 33);
  int val;
  EXPECT_LT(0, op_log_manager.Get(0, 1, &val));  // >0 for found.
  EXPECT_EQ(22, val);
  DenseRow<int> row;
  EXPECT_LT(0, op_log_manager.GetRow(0, &row));  // >0 for found.

  // Now insert kThreadCacheSize other rows to evict row 0.
  InsertThreadCacheRowOf3(&op_log_manager, 1, 11, 22, 33);
  InsertThreadCacheRowOf3(&op_log_manager, 2, 11, 22, 33);
  InsertThreadCacheRowOf3(&op_log_manager, 3, 11, 22, 33);
  EXPECT_EQ(0, op_log_manager.GetRow(0, &row));  // 0 for not found.
}

TEST_F(OpLogManagerTest, GetWithStaleness) {
  OpLogManager<DenseRow, int> op_log_manager(op_log_config_, stats_obj_, kTableID);
  int val;
  int32_t row_iter;
  InsertThreadCacheRowOf3(&op_log_manager, 0, 11, 22, 33, 5);
  EXPECT_LT(0, op_log_manager.Get(0, 0, &val, &row_iter));  // >0 for found.
  EXPECT_EQ(5, row_iter);
  DenseRow<int> row;
  EXPECT_LT(0, op_log_manager.GetRow(0, &row));  // >0 for found.
}

TEST_F(OpLogManagerTest, ReadMyWrite) {
  OpLogManager<DenseRow, int> op_log_manager(op_log_config_, stats_obj_, kTableID);
  InsertThreadCacheRowOf3(&op_log_manager, 0, 11, 22, 33, 5);
  EXPECT_EQ(0, op_log_manager.Put(0, 1, 3));
  int val;
  EXPECT_LT(0, op_log_manager.Get(0, 1, &val));  // >0 for found.
  EXPECT_EQ(3, val);
}

TEST_F(OpLogManagerTest, SerializationDeserialization) {
  OpLogManager<DenseRow, int> op_log_manager(op_log_config_, stats_obj_, kTableID);
  EXPECT_EQ(0, op_log_manager.Put(0, 1, 3));
  EXPECT_EQ(0, op_log_manager.Put(0, 2, 5));
  EXPECT_EQ(0, op_log_manager.Put(0, 3, 7));
  EXPECT_EQ(1, op_log_manager.Inc(0, 4, 9));

  // Actual serialization and deserialization.
  boost::shared_array<uint8_t> op_log_bytes;
  CHECK_GT(op_log_manager.SerializeOpLogTable(&op_log_bytes), 0);
  std::vector<EntryOpExtended<int> > op_logs;
  EXPECT_EQ(0, DeserializeOpLogs<int>(op_log_bytes, &op_logs));

  // For simplicity we enforce the order though it's not required.
  EXPECT_EQ(4, op_logs.size());
  EXPECT_TRUE(VerifyEntryOpExtended(op_logs[3], 0, 1, OpType::PUT, 3));
  EXPECT_TRUE(VerifyEntryOpExtended(op_logs[2], 0, 2, OpType::PUT, 5));
  EXPECT_TRUE(VerifyEntryOpExtended(op_logs[1], 0, 3, OpType::PUT, 7));
  EXPECT_TRUE(VerifyEntryOpExtended(op_logs[0], 0, 4, OpType::INC, 9));
}

TEST_F(OpLogManagerTest, SerializationDeserializationByServer_0) {
  OpLogManager<DenseRow, int> op_log_manager(op_log_config_, stats_obj_, kTableID);
  EXPECT_EQ(0, op_log_manager.Put(0, 1, 3));
  EXPECT_EQ(0, op_log_manager.Put(0, 2, 5));
  EXPECT_EQ(0, op_log_manager.Put(1, 3, 7));
  EXPECT_EQ(1, op_log_manager.Inc(1, 4, 9));
  EXPECT_EQ(1, op_log_manager.Inc(2, 4, 9));

  TablePartitioner& partitioner = TablePartitioner::GetInstance();
  int num_servers = 2;
  partitioner.Init(num_servers);
  std::vector<boost::shared_array<uint8_t> > op_log_bytes_by_server;
  std::vector<int> num_bytes_by_server;
  op_log_manager.SerializeOpLogTableByServer(&op_log_bytes_by_server,
      &num_bytes_by_server);

  {
    // Server 0 should have row 0, 2
    std::vector<EntryOpExtended<int> > op_logs;
    EXPECT_EQ(0, DeserializeOpLogs<int>(op_log_bytes_by_server[0], &op_logs));
    EXPECT_EQ(3, op_logs.size());
    EXPECT_TRUE(VerifyEntryOpExtended(op_logs[2], 0, 1, OpType::PUT, 3));
    EXPECT_TRUE(VerifyEntryOpExtended(op_logs[1], 0, 2, OpType::PUT, 5));
    EXPECT_TRUE(VerifyEntryOpExtended(op_logs[0], 2, 4, OpType::INC, 9));
  }

  {
    // Server 1 should have row 1
    std::vector<EntryOpExtended<int> > op_logs;
    EXPECT_EQ(0, DeserializeOpLogs<int>(op_log_bytes_by_server[1], &op_logs));
    EXPECT_EQ(2, op_logs.size());
    EXPECT_TRUE(VerifyEntryOpExtended(op_logs[1], 1, 3, OpType::PUT, 7));
    EXPECT_TRUE(VerifyEntryOpExtended(op_logs[0], 1, 4, OpType::INC, 9));
  }
}

TEST_F(OpLogManagerTest, SerializationDeserializationByServer_1) {
  OpLogManager<DenseRow, int> op_log_manager(op_log_config_, stats_obj_, kTableID);
  EXPECT_EQ(0, op_log_manager.Put(0, 1, 3));
  EXPECT_EQ(0, op_log_manager.Put(0, 2, 5));
  EXPECT_EQ(0, op_log_manager.Inc(2, 4, 9));

  TablePartitioner& partitioner = TablePartitioner::GetInstance();
  int num_servers = 2;
  partitioner.Init(num_servers);
  std::vector<boost::shared_array<uint8_t> > op_log_bytes_by_server;
  std::vector<int> num_bytes_by_server;
  op_log_manager.SerializeOpLogTableByServer(&op_log_bytes_by_server,
      &num_bytes_by_server);
  // No oplog is going to server[1].
  EXPECT_EQ(sizeof(int32_t), num_bytes_by_server[1]);
}

}  // namespace petuum
