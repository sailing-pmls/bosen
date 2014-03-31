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
// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.02.07

#include "petuum_ps/storage/sparse_row.hpp"
#include <gtest/gtest.h>
#include <vector>

namespace petuum {

namespace {

void IncEntry(SparseRow<int>* row, int32_t col_id, int update) {
  row->ApplyInc(col_id, reinterpret_cast<void*>(&update));
}

}  // anonymous namespace

TEST(SparseRowTest, Constructors) {
  SparseRow<int> row;
  EXPECT_EQ(0, row.num_entries());

  // Reading shouldn't add non-zeros.
  int unused = row[5];
  EXPECT_EQ(0, unused);
  EXPECT_EQ(0, row.num_entries());

  IncEntry(&row, 5, 3);
  EXPECT_EQ(1, row.num_entries());
}

TEST(SparseRowTest, Iterator) {
  SparseRow<int> row;
  for (int i = 0; i < 100; ++i) {
    IncEntry(&row, i, i * 10);
  }

  // const_iterator
  for (SparseRow<int>::const_iterator it = row.cbegin();
      !it.is_end(); ++it) {
    int key = it->first;
    EXPECT_EQ(key * 10, it->second);
  }
}

TEST(SparseRowTest, Serialization) {
  SparseRow<int> row;
  IncEntry(&row, 0, 1);
  IncEntry(&row, 1, 2);
  IncEntry(&row, 2, 3);
  std::unique_ptr<uint8_t> out_bytes(new uint8_t[row.SerializedSize()]);
  int num_bytes = row.Serialize(reinterpret_cast<void*>(out_bytes.get()));

  SparseRow<int> recv_row;
  recv_row.Deserialize(reinterpret_cast<void*>(out_bytes.get()), num_bytes);

  EXPECT_EQ(row[0], recv_row[0]);
  EXPECT_EQ(row[1], recv_row[1]);
  EXPECT_EQ(row[2], recv_row[2]);
  for (int i = 3; i < 100; ++i) {
    EXPECT_EQ(0, recv_row[i]);
  }
}

TEST(SparseRowTest, BatchInc) {
  SparseRow<int> row;
  int32_t num_incs = 3;
  std::unique_ptr<int32_t[]> col_ids(new int32_t[num_incs]);
  col_ids[0] = 0;
  col_ids[1] = 1;
  col_ids[2] = 2;
  std::unique_ptr<uint8_t> update_bytes(new uint8_t[sizeof(int) * num_incs]);
  int* update_bytes_int = reinterpret_cast<int*>(update_bytes.get());
  *update_bytes_int = 1;
  ++update_bytes_int;
  *update_bytes_int = 2;
  ++update_bytes_int;
  *update_bytes_int = 3;
  row.ApplyBatchInc(col_ids.get(),
      reinterpret_cast<void*>(update_bytes.get()), num_incs);

  for (SparseRow<int>::const_iterator it = row.cbegin();
      !it.is_end(); ++it) {
    int key = it->first;
    EXPECT_EQ(key + 1, it->second);
  }
}

}   // namespace petuum
