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

#include "petuum_ps/storage/lru_row_storage.hpp"
#include "petuum_ps/storage/dense_row.hpp"
#include <gtest/gtest.h>
#include <boost/optional.hpp>

// For vector creation.
#include <boost/assign/std/vector.hpp>

namespace petuum {

namespace {

// Size of storage for our test.
const int kCapacity = 3;
const int kActiveListSize = 2;

// 1 row access to upgrade a row from inactive list to active list.
const double kNumRowAccessToActive = 1.;

}  // anonymous namespace

class LRURowStorageTest : public testing::Test {
  protected:
    LRURowStorageTest() :
      lru_row_storage_(kCapacity, kActiveListSize, kNumRowAccessToActive) {
    }

    virtual void SetUp() {
    }

    void InsertRowOf3(int index, int col1_val, int col2_val, int col3_val) {
      using namespace boost::assign; // bring 'operator+=()' into scope
      std::vector<int> row_data;
      row_data += col1_val, col2_val, col3_val;
      EXPECT_LE(0, lru_row_storage_.PutRow(index, DenseRow<int>(row_data)));
    }

    // Used by many test cases.
    LRURowStorage<DenseRow, int> lru_row_storage_;
};

// Smoke test on whether the storage was properly constructed.
TEST_F(LRURowStorageTest, Constructors) {
  EXPECT_EQ(kCapacity, lru_row_storage_.get_size());

  // Test copy constructor
  LRURowStorage<DenseRow, int> lru_row_storage2 = lru_row_storage_;
  EXPECT_EQ(kCapacity, lru_row_storage2.get_size());

  // Test constructing using Init().
  LRURowStorage<DenseRow, int> lru_row_storage3;
  lru_row_storage3.Init(kCapacity);
  EXPECT_EQ(kCapacity, lru_row_storage3.get_size());

  // Construct with StorageConfig
  StorageConfig storage_config;
  storage_config.capacity = kCapacity;
  storage_config.lru_params_are_defined = true;
  storage_config.active_list_size = kActiveListSize;
  storage_config.num_row_access_to_active = kNumRowAccessToActive;
  LRURowStorage<DenseRow, int> lru_row_storage4(storage_config);
  EXPECT_EQ(kCapacity, lru_row_storage4.get_size());
  EXPECT_EQ(kActiveListSize, lru_row_storage4.get_active_list_capacity());
}

// Test PutRow() and Get() when number of rows are within capacity.
TEST_F(LRURowStorageTest, WithinCapacity) {
  // Populate with row 0, 1, 2.
  InsertRowOf3(0, 4, 6, 7);
  InsertRowOf3(1, 6, 8, -1);
  InsertRowOf3(2, 1, 3, 5);

  int num_rows = 3;
  EXPECT_EQ(num_rows, lru_row_storage_.get_num_rows());
  EXPECT_TRUE(lru_row_storage_.HasRow(0));
  EXPECT_TRUE(lru_row_storage_.HasRow(1));
  EXPECT_TRUE(lru_row_storage_.HasRow(2));

  // We should not be able to write to a non-existent row. Return code should
  // be 0 (no-op).
  EXPECT_EQ(0, lru_row_storage_.Put(3, 2, 5));

  // Read back some values. Return code >0 implies entry is found.
  // TODO(wdai): We'd like to test reading out-of-bound values as well.
  int val;
  EXPECT_LT(0, lru_row_storage_.Get(0, 1, &val)); EXPECT_EQ(6, val);
  EXPECT_LT(0, lru_row_storage_.Get(1, 2, &val)); EXPECT_EQ(-1, val);
  EXPECT_LT(0, lru_row_storage_.Get(2, 0, &val)); EXPECT_EQ(1, val);

  // Overwrite a row and read back.
  InsertRowOf3(2, 2, 3, 6);
  EXPECT_TRUE(lru_row_storage_.HasRow(2));
  EXPECT_LT(0, lru_row_storage_.Get(2, 0, &val)); EXPECT_EQ(2, val);

  // Increment row 2 column 0 (with value 2) by 4 to value 6.
  EXPECT_LT(0, lru_row_storage_.Inc(2, 0, 4));  // require return code >0.
  EXPECT_LT(0, lru_row_storage_.Get(2, 0, &val)); EXPECT_EQ(6, val);

  // Erase a row.
  lru_row_storage_.Erase(0);
  EXPECT_FALSE(lru_row_storage_.HasRow(0));
}

// Test LRU when there's no access to the rows.
TEST_F(LRURowStorageTest, LRU_0) {
  // Populate with row 0, 1, 2.
  InsertRowOf3(0, 4, 6, 7);
  InsertRowOf3(1, 6, 8, -1);
  InsertRowOf3(2, 1, 3, 5);

  // Since we inserted 4 rows, row 0 should be evicted.
  InsertRowOf3(3, 8, 9, 10);
  int num_rows = 3;
  EXPECT_EQ(num_rows, lru_row_storage_.get_num_rows());
  EXPECT_FALSE(lru_row_storage_.HasRow(0));
  EXPECT_TRUE(lru_row_storage_.HasRow(1));
  EXPECT_TRUE(lru_row_storage_.HasRow(2));
  EXPECT_TRUE(lru_row_storage_.HasRow(3));

  // Test copy constructor.
  LRURowStorage<DenseRow, int> lru_row_storage2 = lru_row_storage_;
  EXPECT_EQ(num_rows, lru_row_storage2.get_num_rows());
  EXPECT_FALSE(lru_row_storage2.HasRow(0));
  EXPECT_TRUE(lru_row_storage2.HasRow(1));
  EXPECT_TRUE(lru_row_storage2.HasRow(2));
  EXPECT_TRUE(lru_row_storage2.HasRow(3));
}

// Test that accesses lower than threshold will not upgrade a row to active
// list.
TEST_F(LRURowStorageTest, LRU_1) {
  // Populate with row 0, 1, 2.
  InsertRowOf3(0, 4, 6, 7);
  InsertRowOf3(1, 6, 8, -1);
  InsertRowOf3(2, 1, 3, 5);

  // Access two entries of row 0, short of 1 effective row access.
  int val;
  EXPECT_LT(0, lru_row_storage_.Get(0, 0, &val)); EXPECT_EQ(4, val);
  EXPECT_LT(0, lru_row_storage_.Get(0, 1, &val)); EXPECT_EQ(6, val);

  // Now touch row 1, 2 so row 0 is at the beginning of the inactive list
  // (least recent)
  EXPECT_LT(0, lru_row_storage_.Get(1, 1, &val)); EXPECT_EQ(8, val);
  EXPECT_LT(0, lru_row_storage_.Get(2, 0, &val)); EXPECT_EQ(1, val);

  // Since we inserted row 4, row 0 should be evicted.
  InsertRowOf3(3, 8, 9, 10);
  EXPECT_FALSE(lru_row_storage_.HasRow(0));
  EXPECT_TRUE(lru_row_storage_.HasRow(1));
  EXPECT_TRUE(lru_row_storage_.HasRow(2));
  EXPECT_TRUE(lru_row_storage_.HasRow(3));
}

// Test that Get accesses reaching threshold will upgrade a row to active
// list.
TEST_F(LRURowStorageTest, LRU_2) {
  // Populate with row 0, 1, 2.
  InsertRowOf3(0, 4, 6, 7);
  InsertRowOf3(1, 6, 8, -1);
  InsertRowOf3(2, 1, 3, 5);

  // Access three entries of row 0, which should upgarde it to active list.
  int val;
  EXPECT_LT(0, lru_row_storage_.Get(0, 0, &val)); EXPECT_EQ(4, val);
  EXPECT_LT(0, lru_row_storage_.Get(0, 1, &val)); EXPECT_EQ(6, val);
  EXPECT_LT(0, lru_row_storage_.Get(0, 2, &val)); EXPECT_EQ(7, val);

  // Now access row 1 and 2, so that if row 0 isn't in active list then it'd
  // be the least recent row in inactive list and should be evicted when
  // inserting row 3.
  EXPECT_LT(0, lru_row_storage_.Get(1, 1, &val)); EXPECT_EQ(8, val);
  EXPECT_LT(0,lru_row_storage_.Get(2, 0, &val)); EXPECT_EQ(1, val);

  // Since row 0 was upgraded to active list,, row 1 should be evicted.
  InsertRowOf3(3, 8, 9, 10);
  EXPECT_TRUE(lru_row_storage_.HasRow(0));
  EXPECT_FALSE(lru_row_storage_.HasRow(1));
  EXPECT_TRUE(lru_row_storage_.HasRow(2));
  EXPECT_TRUE(lru_row_storage_.HasRow(3));
}

// Test that Get/Put accesses reaching threshold will upgrade a row to active
// list.
TEST_F(LRURowStorageTest, LRU_3) {
  // Populate with row 0, 1, 2.
  InsertRowOf3(0, 4, 6, 7);
  InsertRowOf3(1, 6, 8, -1);
  InsertRowOf3(2, 1, 3, 5);

  // Get two entries of row 0 and Put one entry, which should elevate row 0
  // to active list.
  int val;
  EXPECT_LT(0, lru_row_storage_.Get(0, 0, &val)); EXPECT_EQ(4, val);
  EXPECT_LT(0, lru_row_storage_.Get(0, 1, &val)); EXPECT_EQ(6, val);
  EXPECT_LT(0, lru_row_storage_.Put(0, 1, 99));

  // Now access row 1 and 2, so that if row 0 isn't in active list then it'd
  // be the least recent row in inactive list and should be evicted when
  // inserting row 3.
  EXPECT_LT(0, lru_row_storage_.Get(1, 1, &val)); EXPECT_EQ(8, val);
  EXPECT_LT(0, lru_row_storage_.Get(2, 0, &val)); EXPECT_EQ(1, val);

  // Since row 0 was upgraded to active list,, row 1 should be evicted.
  InsertRowOf3(3, 8, 9, 10);
  EXPECT_TRUE(lru_row_storage_.HasRow(0));
  EXPECT_FALSE(lru_row_storage_.HasRow(1));
  EXPECT_TRUE(lru_row_storage_.HasRow(2));
  EXPECT_TRUE(lru_row_storage_.HasRow(3));
}

// Test active list size is always <= kActiveListSize. Use GetRow() to
// increment access count.
TEST_F(LRURowStorageTest, LRU_4) {
  // Populate with row 0, 1, 2.
  InsertRowOf3(0, 4, 6, 7);
  InsertRowOf3(1, 6, 8, -1);
  InsertRowOf3(2, 1, 3, 5);

  // Make a row-wise access to upgrade all 3 rows to active list, but row 0
  // should be pushed into inactive list to keep the active list size limit.
  DenseRow<int> row_data;
  EXPECT_LT(0, lru_row_storage_.GetRow(0, &row_data));
  EXPECT_EQ(4, row_data[0]);
  EXPECT_EQ(6, row_data[1]);
  EXPECT_EQ(7, row_data[2]);
  EXPECT_LT(0, lru_row_storage_.GetRow(1, &row_data));
  EXPECT_EQ(6, row_data[0]);
  EXPECT_EQ(8, row_data[1]);
  EXPECT_EQ(-1, row_data[2]);
  EXPECT_LT(0, lru_row_storage_.GetRow(2, &row_data));
  EXPECT_EQ(1, row_data[0]);
  EXPECT_EQ(3, row_data[1]);
  EXPECT_EQ(5, row_data[2]);

  // Check the active list size.
  EXPECT_EQ(kActiveListSize, lru_row_storage_.get_active_list_size());

  // Access row 0 to move it to the end of inactive list...
  int val;
  EXPECT_LT(0, lru_row_storage_.Get(0, 0, &val)); EXPECT_EQ(4, val);

  // ...but row 0 will still be evicted when we insert row 3, because there
  // is only one row in inactive list.
  InsertRowOf3(3, 8, 9, 10);
  EXPECT_FALSE(lru_row_storage_.HasRow(0));
  EXPECT_TRUE(lru_row_storage_.HasRow(1));
  EXPECT_TRUE(lru_row_storage_.HasRow(2));
  EXPECT_TRUE(lru_row_storage_.HasRow(3));
}

// Test active list size is always <= kActiveListSize. Use PutRow() to
// increment access count.
TEST_F(LRURowStorageTest, LRU_5) {
  // Populate with row 0, 1, 2.
  InsertRowOf3(0, 4, 6, 7);
  InsertRowOf3(1, 6, 8, -1);
  InsertRowOf3(2, 1, 3, 5);

  // Make a row-wise access to upgrade all 3 rows to active list, but row 0
  // should be pushed into inactive list to keep the active list size limit.
  InsertRowOf3(0, 4, 6, 7);
  InsertRowOf3(1, 6, 8, -1);
  InsertRowOf3(2, 1, 3, 5);

  // Check the active list size.
  EXPECT_EQ(kActiveListSize, lru_row_storage_.get_active_list_size());

  // Access row 0 to move it to the end of inactive list...
  EXPECT_LT(0, lru_row_storage_.Put(0, 0, 99));

  // ...but row 0 will still be evicted when we insert row 3, because there
  // is only one row in inactive list.
  InsertRowOf3(3, 8, 9, 10);
  EXPECT_FALSE(lru_row_storage_.HasRow(0));
  EXPECT_TRUE(lru_row_storage_.HasRow(1));
  EXPECT_TRUE(lru_row_storage_.HasRow(2));
  EXPECT_TRUE(lru_row_storage_.HasRow(3));
}

TEST_F(LRURowStorageTest, GetRowInternal) {
  // Populate with row 0, 1, 2.
  InsertRowOf3(0, 4, 6, 7);
  EXPECT_TRUE(lru_row_storage_.GetRowInternal(0));
  DenseRow<int>& row = *lru_row_storage_.GetRowInternal(0);
  EXPECT_EQ(4, row[0]);
  EXPECT_EQ(6, row[1]);
  EXPECT_EQ(7, row[2]);

  // Row 1 does not exist.
  EXPECT_FALSE(lru_row_storage_.GetRowInternal(1));
}

}  // namespace petuum
