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
// Date: 2014.02.01

#include "petuum_ps/storage/process_storage.hpp"
#include "petuum_ps/storage/sparse_row.hpp"
#include "petuum_ps/storage/client_row.hpp"
#include <gtest/gtest.h>
#include <glog/logging.h>
#include <thread>
#include <vector>
#include <random>
#include <utility>
#include <cstdint>

namespace petuum {

typedef SparseRow<int> SparseRowInt;

namespace {

void UpdateEntry(SparseRowInt* row, int32_t col_id, int update) {
  row->ApplyUpdate(col_id, reinterpret_cast<void*>(&update));
}

}  // anonymous namespace


TEST(ProcessStorageTest, SmokeTest) {
  int storage_capacity = 10;
  ProcessStorage storage(storage_capacity);
  RowAccessor acc;
  EXPECT_FALSE(storage.Find(0, &acc));

  SparseRowInt* row = new SparseRowInt;
  UpdateEntry(row, 2, 100);
  UpdateEntry(row, 5, 101);
  UpdateEntry(row, 11, 102);

  ClientRow* client_row = new ClientRow(row);
  int row_id = 1;
  storage.Insert(row_id, client_row, &acc);
  const SparseRowInt& row_ref = acc.Get<SparseRowInt>();
  EXPECT_EQ(100, row_ref[2]);
  EXPECT_EQ(101, row_ref[5]);
  EXPECT_EQ(102, row_ref[11]);
  EXPECT_EQ(0, row_ref[0]);
  EXPECT_TRUE(storage.Find(row_id, &acc));
}

// Just create the same row.
ClientRow* CreateClientRow() {
  SparseRowInt* row = new SparseRowInt;
  UpdateEntry(row, 2, 100);
  UpdateEntry(row, 5, 101);
  UpdateEntry(row, 11, 102);
  ClientRow* client_row = new ClientRow(row);
  return client_row;
}

namespace {

std::atomic<int> seq_number;

int kNumIter = 1e4;

int kNumThreads = sysconf(_SC_NPROCESSORS_ONLN);

}  // anonymous namespace

void MTTestHelper(ProcessStorage* storage) {
  for (int i = 0; i < kNumIter; ++i) {
    RowAccessor acc;
    // the row_id is unique.
    EXPECT_TRUE(storage->Insert(seq_number++, CreateClientRow(), &acc));
  }
}


TEST(ProcessStorageTest, MTTest) {
  int storage_capacity = 10;
  ProcessStorage storage(storage_capacity);
  seq_number = storage_capacity;

  // Fill up the storage.
  for (int i = 0; i < storage_capacity; ++i) {
    RowAccessor acc;
    storage.Insert(i, CreateClientRow(), &acc);
  }

  std::vector<std::thread> thread_pool;
  LOG(INFO) << "Using " << kNumThreads << " threads.";
  for (int i = 0; i < kNumThreads; ++i) {
    thread_pool.emplace_back(MTTestHelper, &storage);
  }
  for (int i = 0; i < kNumThreads; ++i) {
    thread_pool[i].join();
  }
}

}   // namespace petuum
