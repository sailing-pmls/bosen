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
// Date: 2014.01.20

#pragma once

#include <petuum_ps_common/util/lockable.hpp>
#include <pthread.h>
#include <atomic>
#include <mutex>
#include <boost/utility.hpp>

namespace petuum {

// Read-write mutex, almost equivalent to boost::shared_mutex (but without
// timed locking). Benchmark shows that this is about 2x faster than
// boost::shared_mutex on Ubuntu 12.04.
//
// Usage:
// {
//    SharedMutex rw_mutex;
//    boost::unique_lock<SharedMutex> lock(rw_mutex);
//    // Do things with exclusive lock
// }
// {
//    SharedMutex rw_mutex;
//    boost::shared_lock<SharedMutex> lock(rw_mutex);
//    // Do things with exclusive lock
// }
//
// Code adapted from
// http://boost.2283326.n4.nabble.com/boost-shared-mutex-performance-td2659061.html
class SharedMutex : public Lockable {
public:
  SharedMutex();

  ~SharedMutex();

  void lock();

  bool try_lock();

  void unlock();

  void lock_shared();

  bool try_lock_shared();

  void unlock_shared();

protected:
  pthread_rwlock_t rw_lock_;
};


// Provide recursive lock with write lock counting. Note the function override
// hides those in SharedMutex.
class RecursiveSharedMutex: public SharedMutex {
public:
  RecursiveSharedMutex();

  ~RecursiveSharedMutex();

  void lock();

  bool try_lock();

  void unlock();

  void lock_shared();

  bool try_lock_shared();

  void unlock_shared();

private:
  unsigned int write_lock_count_;

  // ID of the writer thread; set during the write lock.
  pthread_t writer_id;
};


class SpinMutex : public Lockable {
public:
  SpinMutex() {
    lock_.clear();
  }

  inline void lock() {
    while (lock_.test_and_set(std::memory_order_acquire)) { }
  }

  inline void unlock() {
    lock_.clear(std::memory_order_release);
  }

  inline bool try_lock() {
    return !lock_.test_and_set(std::memory_order_acquire);
  }

private:
  std::atomic_flag lock_;
} __attribute__((aligned(64)));


// It takes an acquired lock and unlock it in destructor.
template<typename MUTEX = std::mutex>
class Unlocker : boost::noncopyable {
public:
  Unlocker() : lock_(0) { }

  ~Unlocker() {
    if (lock_ != 0) lock_->unlock();
  }

  // lock must have been locked already. It does not take ownership.
  inline void SetLock(MUTEX* lock) {
    if (lock_ != 0) lock_->unlock();
    lock_ = lock;
  }

  // Release the lock.
  inline void Release() {
    lock_ = 0;
  }

  inline MUTEX* GetAndRelease() {
    MUTEX* tmp_lock = lock_;
    lock_ = 0;
    return tmp_lock;
  }

private:
  MUTEX* lock_;
};

}   // namespace petuum
