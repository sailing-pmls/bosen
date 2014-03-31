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
// Date: 2014.03.23

#include "petuum_ps/storage/sorted_vector_map_row.hpp"
#include <gtest/gtest.h>
#include <vector>
#include <utility>

namespace petuum {

namespace {

void IncEntry(SortedVectorMapRow<int>* row, int32_t col_id, int update) {
  row->ApplyInc(col_id, reinterpret_cast<void*>(&update));
}

}  // anonymous namespace

TEST(SortedVectorMapRowTest, Constructors) {
  SortedVectorMapRow<int> row;
  EXPECT_EQ(0, row.num_entries());

  EXPECT_EQ(0, row.num_entries());

  IncEntry(&row, 5, 3);
  EXPECT_EQ(1, row.num_entries());
}

TEST(SortedVectorMapRowTest, Iterator) {
  SortedVectorMapRow<int> row;
  int num_items = 11;
  for (int i = 0; i < num_items; ++i) {
    IncEntry(&row, i, i * 10);
  }

  // const_iterator
  int i = num_items;
  for (SortedVectorMapRow<int>::const_iterator it = row.cbegin();
      !it.is_end(); ++it) {
    int key = it->first;
    EXPECT_EQ(--i, key);  // sorted order.
    EXPECT_EQ(key * 10, it->second);
  }
}

TEST(SortedVectorMapRowTest, MaintainOrder) {
  SortedVectorMapRow<int> row;
  int num_items = 5;
  int original_val = 10;
  for (int i = 0; i < num_items; ++i) {
    IncEntry(&row, i, original_val);
  }

  // Now substract varying amount from each entry.
  IncEntry(&row, 0, -9);
  IncEntry(&row, 4, -2);
  IncEntry(&row, 2, 2);
  IncEntry(&row, 3, 4);
  EXPECT_EQ(num_items, row.num_entries());

  {
    // New order of columns should be: 3, 2, 1, 4, 0
    std::vector<std::pair<int, int> > expected_order(num_items);
    expected_order[0] = std::make_pair(3, 14);
    expected_order[1] = std::make_pair(2, 12);
    expected_order[2] = std::make_pair(1, 10);
    expected_order[3] = std::make_pair(4, 8);
    expected_order[4] = std::make_pair(0, 1);
    int i = 0;
    for (SortedVectorMapRow<int>::const_iterator it = row.cbegin();
        !it.is_end(); ++it) {
      int key = it->first;
      EXPECT_EQ(expected_order[i].first, key);  // sorted order.
      EXPECT_EQ(expected_order[i].second, it->second);
      ++i;
    }
  }

  // Zero element needs to be removed.
  IncEntry(&row, 2, -12);
  EXPECT_EQ(num_items - 1, row.num_entries());
  {
    // New order of columns should be: 3, 1, 4, 0
    std::vector<std::pair<int, int> > expected_order(num_items - 1);
    expected_order[0] = std::make_pair(3, 14);
    expected_order[1] = std::make_pair(1, 10);
    expected_order[2] = std::make_pair(4, 8);
    expected_order[3] = std::make_pair(0, 1);
    int i = 0;
    for (SortedVectorMapRow<int>::const_iterator it = row.cbegin();
        !it.is_end(); ++it) {
      int key = it->first;
      EXPECT_EQ(expected_order[i].first, key);  // sorted order.
      EXPECT_EQ(expected_order[i].second, it->second);
      ++i;
    }
  }
}

TEST(SortedVectorMapRowTest, ApplyIncBatch) {
  SortedVectorMapRow<int> row;
  int num_incs = 5;
  int original_val = 10;
  {
    // Set col 0 ~ num_incs - 1 to have value 10.
    std::unique_ptr<int32_t[]> col_ids(new int32_t[num_incs]);
    for (int i = 0; i < num_incs; ++i) {
      col_ids[i] = i;
    }
    std::unique_ptr<uint8_t> update_bytes(new uint8_t[sizeof(int) * num_incs]);
    int* update_bytes_int = reinterpret_cast<int*>(update_bytes.get());
    for (int i = 0; i < num_incs; ++i) {
      *(update_bytes_int + i) = original_val;
    }
    row.ApplyBatchInc(col_ids.get(),
        reinterpret_cast<void*>(update_bytes.get()), num_incs);
    EXPECT_EQ(num_incs, row.num_entries());
  }

  {
    // Verify column 0 ~ (num_incs - 1) all have value 10.
    int i = 0;
    for (SortedVectorMapRow<int>::const_iterator it = row.cbegin();
        !it.is_end(); ++it) {
      int key = it->first;
      EXPECT_EQ(i, key);  // insert order.
      EXPECT_EQ(original_val, it->second);
      ++i;
    }
  }

  {
    // Now substract varying amount from each entry.
    std::unique_ptr<int32_t[]> col_ids(new int32_t[num_incs - 1]);
    col_ids[0] = 0;
    col_ids[1] = 4;
    col_ids[2] = 2;
    col_ids[3] = 3;
    std::unique_ptr<uint8_t> update_bytes(
        new uint8_t[sizeof(int) * (num_incs - 1)]);
    int* update_bytes_int = reinterpret_cast<int*>(update_bytes.get());
    *(update_bytes_int + 0) = -9;
    *(update_bytes_int + 1) = -2;
    *(update_bytes_int + 2) = 2;
    *(update_bytes_int + 3) = 4;
    row.ApplyBatchInc(col_ids.get(),
        reinterpret_cast<void*>(update_bytes.get()), num_incs - 1);
    EXPECT_EQ(num_incs, row.num_entries());
  }

  {
    // New order of columns should be: 3, 2, 1, 4, 0
    std::vector<std::pair<int, int> > expected_order(num_incs);
    expected_order[0] = std::make_pair(3, 14);
    expected_order[1] = std::make_pair(2, 12);
    expected_order[2] = std::make_pair(1, 10);
    expected_order[3] = std::make_pair(4, 8);
    expected_order[4] = std::make_pair(0, 1);
    int i = 0;
    for (SortedVectorMapRow<int>::const_iterator it = row.cbegin();
        !it.is_end(); ++it) {
      int key = it->first;
      EXPECT_EQ(expected_order[i].first, key);  // sorted order.
      EXPECT_EQ(expected_order[i].second, it->second);
      ++i;
    }
  }
}

TEST(SortedVectorMapRowTest, Serialization) {
  SortedVectorMapRow<int> row;
  IncEntry(&row, 0, 1);
  IncEntry(&row, 1, 2);
  IncEntry(&row, 2, 3);
  std::unique_ptr<uint8_t> out_bytes(new uint8_t[row.SerializedSize()]);
  int num_bytes = row.Serialize(reinterpret_cast<void*>(out_bytes.get()));

  SortedVectorMapRow<int> recv_row;
  recv_row.Deserialize(reinterpret_cast<void*>(out_bytes.get()), num_bytes);

  {
    int num_incs = 3;
    std::vector<std::pair<int, int> > expected_order(num_incs);
    expected_order[0] = std::make_pair(2, 3);
    expected_order[1] = std::make_pair(1, 2);
    expected_order[2] = std::make_pair(0, 1);
    int i = 0;
    for (SortedVectorMapRow<int>::const_iterator it = recv_row.cbegin();
        !it.is_end(); ++it) {
      int key = it->first;
      EXPECT_EQ(expected_order[i].first, key);  // sorted order.
      EXPECT_EQ(expected_order[i].second, it->second);
      ++i;
    }
  }
}

}   // namespace petuum
