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
