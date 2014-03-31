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
// Date: 2014.01.24

#include "petuum_ps/storage/clock_lru.hpp"
#include <libcuckoo/cuckoohash_map.hh>
#include <gtest/gtest.h>
#include <glog/logging.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <random>
#include <utility>
#include <cstdint>

namespace petuum {

namespace {

int kNumThreads = sysconf(_SC_NPROCESSORS_ONLN);

}  // anonymous namespace

TEST(ClockLRUTest, SmokeTest) {
  int lru_capacity = 10;
  ClockLRU clock_lru(lru_capacity);
  EXPECT_LE(0, clock_lru.Insert(0));

  // Inserting up to lru_capacity keys should be fine.
  for (int i = 1; i < lru_capacity; ++i) {
    EXPECT_LE(0, clock_lru.Insert(i));
  }
}

TEST(ClockLRUTest, EvictTest) {
  int lru_capacity = 10;
  // row_id --> slot #
  cuckoohash_map<int32_t, int32_t> storage_map(lru_capacity);
  ClockLRU clock_lru(lru_capacity);
  // Inserting up to lru_capacity keys should be fine.
  for (int i = 0; i < lru_capacity; ++i) {
    int32_t slot = clock_lru.Insert(i);
    EXPECT_LE(0, slot);
    storage_map.insert(i, slot);
  }

  for (int i = 0; i < lru_capacity; ++i) {
    int evict_candidate = clock_lru.FindOneToEvict();
    // Now ProcessStorage checks ref count.
    int32_t slot;
    EXPECT_TRUE(storage_map.find(evict_candidate, slot));
    if (i % 2 == 0) {
      storage_map.erase(evict_candidate);
      clock_lru.Evict(slot);
    } else {
      clock_lru.NoEvict(slot);
    }
  }

  // Now the odd number ones are left.
  for (int i = 1; i < lru_capacity/2; i += 2) {
    int32_t slot;
    EXPECT_TRUE(storage_map.find(i, slot));
    clock_lru.Reference(slot);
  }

  // Now we try to evict. It should evict the ones greater than lru_capacity/2
  int evict_candidate = clock_lru.FindOneToEvict();
  EXPECT_LE(lru_capacity/2, evict_candidate);
  int32_t slot;
  EXPECT_TRUE(storage_map.find(evict_candidate, slot));
  storage_map.erase(evict_candidate);
  clock_lru.Evict(slot);
}


/*
// Insert integer [begin, end) to lru and expect success.
void MTInsertTestHelper(ClockLRU* clock_lru, int begin, int end) {
  for (int i = begin; i < end; ++i) {
    EXPECT_EQ(0, clock_lru->Insert(i));
  }
}

// Multi-threaded Insert Test.
TEST(ClockLRUTest, MTInsertTest) {
  int lru_capacity = 1e5;
  cuckoohash_map<int32_t, std::pair<void*, int32_t> >
    storage_map(lru_capacity);
  ClockLRU clock_lru(lru_capacity, &storage_map);

  int num_items_per_thread = lru_capacity / kNumThreads;

  std::vector<std::thread> thread_pool;
  for (int i = 0; i < kNumThreads; ++i) {
    thread_pool.emplace_back(MTInsertTestHelper, &clock_lru,
        i * num_items_per_thread, (i+1) * num_items_per_thread);
  }

  for (int i = 0; i < kNumThreads; ++i) {
    thread_pool[i].join();
  }
}

*/

namespace {

std::atomic<int> seq_number;

// A good choice of this parameter depends on lru_capacity and eviction_rate.
int kNumItemsToReference = 4;

// (num iter doing referencing) / (num iter evicting)
float kEvictionRate = 0.9;

}   // anonymous namespace

// Evict 1 and Insert 1 repeatedly
void MTEvictTestHelper(ClockLRU* clock_lru,
    cuckoohash_map<int32_t, int32_t>* storage_map,
    const std::vector<int>& frequent_items, int capacity) {
  int num_iter = 1e3;
  std::random_device rd;
  std::mt19937 engine(rd());
  std::uniform_real_distribution<float> dist(0.0, 1.0);
  int num_freq_items = frequent_items.size();
  std::uniform_int_distribution<> int_dist(0, num_freq_items - 1);
  for (int i = 0; i < num_iter; ++i) {
    int evict_candidate = clock_lru->FindOneToEvict();
    // Now ProcessStorage checks ref count.
    int32_t slot;
    EXPECT_TRUE(storage_map->find(evict_candidate, slot));
    if (dist(engine) < kEvictionRate) {
      EXPECT_TRUE(storage_map->erase(evict_candidate));
      clock_lru->Evict(slot);
      // Insert one that's not in the initial range.
      int32_t insert_row_id = seq_number++;
      int32_t slot_to_insert = clock_lru->Insert(insert_row_id);
      EXPECT_TRUE(storage_map->insert(insert_row_id, insert_row_id));
    } else {
      clock_lru->NoEvict(slot);
      // Touch at most 10 items.
      int begin = int_dist(engine);
      for (int j = begin; j < begin + kNumItemsToReference; ++j) {
        int32_t row_to_ref = frequent_items[j % num_freq_items];
        int32_t slot_reference;
        storage_map->find(row_to_ref, slot_reference);
        clock_lru->Reference(slot_reference);
      }
    }
  }
}

TEST(ClockLRUTest, MTEvictTest) {
  int lru_capacity = 1e4;
  seq_number = lru_capacity;
  // row_id --> slot #
  cuckoohash_map<int32_t, int32_t> storage_map(lru_capacity);
  ClockLRU clock_lru(lru_capacity);
  // Fill it up.
  for (int i = 0; i < lru_capacity; ++i) {
    int32_t slot = clock_lru.Insert(i);
    EXPECT_LE(0, slot);
    storage_map.insert(i, slot);
  }

  // Pick a range to reference frequently, most of which should stay in the
  // cache at the end.
  std::vector<int> frequent_items;
  for (int i = 1000; i < 1100; ++i) {
    frequent_items.push_back(i);
  }

  std::vector<std::thread> thread_pool;
  LOG(INFO) << "Using " << kNumThreads << " threads.";
  for (int i = 0; i < kNumThreads; ++i) {
    thread_pool.emplace_back(MTEvictTestHelper, &clock_lru, &storage_map,
        std::ref(frequent_items), lru_capacity);
  }
  for (int i = 0; i < kNumThreads; ++i) {
    thread_pool[i].join();
  }

  // Check frequent_items
  int alive_count = 0;
  for (const auto& item : frequent_items) {
    int32_t slot;
    storage_map.find(item, slot);
    if (clock_lru.HasRow(item, slot)) {
      alive_count++;
    }
  }

  // Expect 80% to survive.
  double survival_ratio = static_cast<double>(alive_count)
    / frequent_items.size();
  LOG(INFO) << survival_ratio * 100 << "\% survived";
  EXPECT_GE(survival_ratio, 0.8);
}

}  // namespace petuum
