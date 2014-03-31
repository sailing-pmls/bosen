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

#include "petuum_ps/storage/dense_row.hpp"
#include <boost/shared_array.hpp>
#include <gtest/gtest.h>


namespace petuum {

TEST(DenseRowTest, Constructors) {
  int num_cols = 100;
  int init_iter = 10;
  DenseRow<int> row(num_cols);

  EXPECT_EQ(num_cols, row.get_num_columns());
  EXPECT_EQ(0, row.get_iteration());

  DenseRow<int> row2(num_cols, init_iter);
  EXPECT_EQ(num_cols, row2.get_num_columns());
  EXPECT_EQ(init_iter, row2.get_iteration());
};

TEST(DenseRowTest, EntryAccessMutation) {
  int num_cols = 100;
  DenseRow<int> row(num_cols);

  EXPECT_EQ(0, row[3]);
  row[3] = 5;
  EXPECT_EQ(5, row[3]);
};

TEST(DenseRowTest, Serialization) {
  int num_cols = 100;
  DenseRow<int> row(num_cols);
  row[0] = 1;
  row[1] = 2;
  row[2] = 3;
  boost::shared_array<uint8_t> out_bytes;
  int num_bytes = row.Serialize(out_bytes);

  DenseRow<int> recv_row;
  recv_row.Deserialize(out_bytes, num_bytes);

  EXPECT_EQ(row.get_num_columns(), recv_row.get_num_columns());
  EXPECT_EQ(row[0], recv_row[0]);
  EXPECT_EQ(row[1], recv_row[1]);
  EXPECT_EQ(row[2], recv_row[2]);
  for (int i = 3; i < num_cols; ++i) {
    EXPECT_EQ(0, recv_row[i]);
  }
}

}  // namespace petuum
