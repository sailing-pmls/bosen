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

#include "petuum_ps/util/striped_lock.hpp"
#include "petuum_ps/util/lock.hpp"
#include <gtest/gtest.h>
#include <time.h>
#include <thread>
#include <vector>
#include <stdlib.h>

namespace petuum {

namespace {

int kLockPoolSize = 100;

}  // anonymous namespace

// Check lock are released properly.
TEST(StripedLockTest, SmokeTest) {
  {
    StripedLock<int> locks(kLockPoolSize);
    for (int i = 0; i < 100; ++i) {
      locks.Lock(6);
      locks.Unlock(6);
    }

    for (int i = 0; i < 100; ++i) {
      Unlocker<> unlocker;
      locks.Lock(6, &unlocker);
    }
  }

  {
    // SpinMutex
    StripedLock<int, SpinMutex> locks(kLockPoolSize);
    for (int i = 0; i < 100; ++i) {
      locks.Lock(6);
      locks.Unlock(6);
    }

    for (int i = 0; i < 100; ++i) {
      Unlocker<SpinMutex> unlocker;
      locks.Lock(6, &unlocker);
    }
  }
}

namespace {

int kNumThreads = sysconf(_SC_NPROCESSORS_ONLN);

}  // anonymous namespace

void MTTestHelper(StripedLock<int>* locks, std::vector<int64_t>* shared_vals,
    int num_iter) {
  for (int i = 0; i < num_iter; ++i) {
    Unlocker<> unlocker;
    int r = rand_r() % 3;
    locks->Lock(r, &unlocker);
    (*shared_vals)[r]++;
  }
}

TEST(StripedLockTest, MTTest) {
  srand(time(NULL));
  StripedLock<int> locks(kLockPoolSize);
  std::vector<std::thread> thread_pool;
  LOG(INFO) << "Using " << kNumThreads << " threads.";
  int num_iter = 1e5;
  std::vector<int64_t> shared_vals(3);
  for (int i = 0; i < kNumThreads; ++i) {
    thread_pool.emplace_back(MTTestHelper, &locks, &shared_vals, num_iter);
  }
  for (int i = 0; i < kNumThreads; ++i) {
    thread_pool[i].join();
  }
  for (int i = 0; i < 3; ++i) {
    EXPECT_NEAR(num_iter*kNumThreads/3. / shared_vals[i], 1., 1e-1);
  }
}

}  // namespace petuum
