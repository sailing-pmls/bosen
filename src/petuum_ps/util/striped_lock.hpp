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
// Date: 2014.01.23

#pragma once

#include "petuum_ps/util/lock.hpp"
#include "petuum_ps/util/lockable.hpp"
#include <sys/sysinfo.h>  // get_nprocs_conf
#include <mutex>
#include <cstdint>
#include <boost/scoped_array.hpp>
#include <boost/utility.hpp>
#include <glog/logging.h>

namespace petuum {

namespace {

// We will use lock pool of size (num_threads * kLockPoolSizeExpansionFactor).
int32_t kLockPoolSizeExpansionFactor = 20;

}   // anonymous namespace

// StripedLock does not support scoped lock. MUTEX must implement Lockable
// interface.
template<typename K,
typename MUTEX = std::mutex,
typename HASH = std::hash<K> >
class StripedLock : boost::noncopyable {
public:
  // Determine number of locks based on number of cores.
  StripedLock() :
    StripedLock(get_nprocs_conf() * kLockPoolSizeExpansionFactor) { }

  // Initialize with number of locks in the pool.
  explicit StripedLock(int lock_pool_size) :
    lock_pool_size_(lock_pool_size),
    lock_pool_(new MUTEX[lock_pool_size_]) {
    //VLOG(0) << "Lock pool size: " << lock_pool_size_;
    }

  // Lock index idx.
  inline void Lock(K idx) {
    int lock_idx = hasher(idx) % lock_pool_size_;
    lock_pool_[lock_idx].lock();
  }

  // Lock index idx, and let unlocker unlock it later on.
  inline void Lock(K idx, Unlocker<MUTEX>* unlocker) {
    int lock_idx = hasher(idx) % lock_pool_size_;
    lock_pool_[lock_idx].lock();
    unlocker->SetLock(&lock_pool_[lock_idx]);
  }

  // Lock index idx.
  inline bool TryLock(K idx) {
    int lock_idx = hasher(idx) % lock_pool_size_;
    return lock_pool_[lock_idx].try_lock();
  }

  // Lock index idx.
  inline bool TryLock(K idx, Unlocker<MUTEX>* unlocker) {
    int lock_idx = hasher(idx) % lock_pool_size_;
    if (lock_pool_[lock_idx].try_lock()) {
      unlocker->SetLock(&lock_pool_[lock_idx]);
      return true;
    }
    return false;
  }

  // Unlock.
  inline void Unlock(K idx) {
    int lock_idx = hasher(idx) % lock_pool_size_;
    lock_pool_[lock_idx].unlock();
  }

private:
  // Use default HASH function.
  static HASH hasher;

  // Size of the pool. Currently does not support resizing.
  const int32_t lock_pool_size_;

  // pool of mutexes.
  boost::scoped_array<MUTEX> lock_pool_;
};

}  // namespace petuum
