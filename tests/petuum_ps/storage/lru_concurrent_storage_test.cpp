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
#include "petuum_ps/storage/lru_concurrent_storage.hpp"
#include "petuum_ps/storage/dense_row.hpp"
#include <gtest/gtest.h>
// For vector creation.
#include <boost/assign/std/vector.hpp>

namespace petuum {

namespace {

const int kCapacity = 3;

}  // anonymous namespace

class ConcurrentRowStorageTest : public testing::Test {
  protected:
    ConcurrentRowStorageTest() :
      concurrent_row_storage_(kCapacity) {
    }

    virtual void SetUp() {
    }

    void InsertRowOf3(int index, int col1_val, int col2_val, int col3_val) {
      using namespace boost::assign; // bring 'operator+=()' into scope
      std::vector<int> row_data;
      row_data += col1_val, col2_val, col3_val;
      EXPECT_LE(0, concurrent_row_storage_.PutRow(index,
            DenseRow<int>(row_data)));
    }

    // Used by many test cases.
    ConcurrentRowStorage<DenseRow, int> concurrent_row_storage_;
};

// smoke test.
TEST_F(ConcurrentRowStorageTest, Constructor) {
  EXPECT_EQ(kCapacity, concurrent_row_storage_.get_size());

  // Test copy constructor
  ConcurrentRowStorage<DenseRow, int> concurrent_row_storage2 =
    concurrent_row_storage_;
  EXPECT_EQ(kCapacity, concurrent_row_storage2.get_size());

  // Construct with StorageConfig
  StorageConfig storage_config;
  storage_config.capacity = kCapacity;
  ConcurrentRowStorage<DenseRow, int> concurrent_row_storage3(storage_config);
  EXPECT_EQ(kCapacity, concurrent_row_storage3.get_size());
}

// Test PutRow() and Get() when number of rows are within capacity.
TEST_F(ConcurrentRowStorageTest, WithinCapacity) {
  // Populate with row 0, 1, 2.
  InsertRowOf3(0, 4, 6, 7);
  InsertRowOf3(1, 6, 8, -1);
  InsertRowOf3(2, 1, 3, 5);

  int num_rows = 3;
  EXPECT_EQ(num_rows, concurrent_row_storage_.get_num_rows());
  EXPECT_TRUE(concurrent_row_storage_.HasRow(0));
  EXPECT_TRUE(concurrent_row_storage_.HasRow(1));
  EXPECT_TRUE(concurrent_row_storage_.HasRow(2));

  // We should not be able to write to a non-existent row. Return 0 for no-op.
  EXPECT_EQ(0, concurrent_row_storage_.Put(3, 2, 5));

  // Read back some values.
  // TODO(wdai): We'd like to test reading out-of-bound values as well.
  int val;
  EXPECT_LT(0, concurrent_row_storage_.Get(0, 1, &val)); EXPECT_EQ(6, val);
  EXPECT_LT(0, concurrent_row_storage_.Get(1, 2, &val)); EXPECT_EQ(-1, val);
  EXPECT_LT(0, concurrent_row_storage_.Get(2, 0, &val)); EXPECT_EQ(1, val);

  // Overwrite a row and read back.
  InsertRowOf3(2, 2, 3, 6);
  EXPECT_TRUE(concurrent_row_storage_.HasRow(2));
  EXPECT_LT(0, concurrent_row_storage_.Get(2, 0, &val)); EXPECT_EQ(2, val);

  // Increment row 2 column 0 (with value 2) by 4 to value 6.
  EXPECT_LT(0, concurrent_row_storage_.Inc(2, 0, 4));
  EXPECT_LT(0, concurrent_row_storage_.Get(2, 0, &val)); EXPECT_EQ(6, val);

  // Restore it.
  EXPECT_LT(0, concurrent_row_storage_.Inc(2, 0, -4));

  // Erase a row.
  concurrent_row_storage_.Erase(0);
  EXPECT_FALSE(concurrent_row_storage_.HasRow(0));

  // Exercise GetRow
  DenseRow<int> v;
  EXPECT_LT(0, concurrent_row_storage_.GetRow(2, &v));
  EXPECT_EQ(2, v[0]);
  EXPECT_EQ(3, v[1]);
  EXPECT_EQ(6, v[2]);

  // Test copy constructor
  ConcurrentRowStorage<DenseRow, int> concurrent_row_storage2 =
    concurrent_row_storage_;
  EXPECT_LT(0, concurrent_row_storage2.GetRow(2, &v));
  EXPECT_EQ(2, v[0]);
  EXPECT_EQ(3, v[1]);
  EXPECT_EQ(6, v[2]);
}

TEST_F(ConcurrentRowStorageTest, OutOfCapacity) {
  // Populate with row 0, 1, 2.
  InsertRowOf3(0, 4, 6, 7);
  InsertRowOf3(1, 6, 8, -1);
  InsertRowOf3(2, 1, 3, 5);

  int num_rows = 3;
  EXPECT_EQ(num_rows, concurrent_row_storage_.get_num_rows());

  // This should return error code <0 (out of capacity).
  using namespace boost::assign; // bring 'operator+=()' into scope
  std::vector<int> row_data;
  row_data += 5, 6, -10;
  // this test will not fail
  // We should stay within capacity.
  EXPECT_EQ(num_rows, concurrent_row_storage_.get_num_rows());
}



void reading(int id, ConcurrentRowStorage<DenseRow, int>* concurrent_row_storage_)
{
  using namespace boost::assign; // bring 'operator+=()' into scope
  DenseRow<int> v;
  for (int i = 0; i < 100; i++)
  {
    concurrent_row_storage_->GetRow(id%3, &v);
    std::vector<int> row_data;
    row_data += i+4, i+6, i+7;
    concurrent_row_storage_->PutRow(id%3,
          DenseRow<int>(row_data));
  }
}

TEST_F(ConcurrentRowStorageTest, multi_threading_test) {
  InsertRowOf3(0, 4, 6, 7);
  InsertRowOf3(1, 6, 8, -1);
  InsertRowOf3(2, 1, 3, 5);

  int num_rows = 3;
  EXPECT_EQ(0, concurrent_row_storage_.Put(3, 2, 5));
  boost::thread* workers[100];

  for (int i = 0; i < 99; i++)
  {
    workers[i] = new boost::thread(reading, i, &concurrent_row_storage_);
  }

  for (int i = 0; i < 99; i++)
  {
    workers[i]->join();
  }

  EXPECT_EQ(num_rows, concurrent_row_storage_.get_num_rows());

}


}  // namespace petuum
