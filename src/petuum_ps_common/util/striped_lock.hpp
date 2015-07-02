// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.01.23

#pragma once

#include <petuum_ps_common/util/lock.hpp>
#include <petuum_ps_common/util/lockable.hpp>
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
    int lock_idx = GetLockIndex(idx);
    lock_pool_[lock_idx].lock();
  }

  // Lock index idx, and let unlocker unlock it later on.
  inline void Lock(K idx, Unlocker<MUTEX>* unlocker) {
    int lock_idx = GetLockIndex(idx);
    lock_pool_[lock_idx].lock();
    unlocker->SetLock(&lock_pool_[lock_idx]);
  }

  // Lock index idx.
  inline bool TryLock(K idx) {
    int lock_idx = GetLockIndex(idx);
    return lock_pool_[lock_idx].try_lock();
  }

  // Lock index idx.
  inline bool TryLock(K idx, Unlocker<MUTEX>* unlocker) {
    int lock_idx = GetLockIndex(idx);
    if (lock_pool_[lock_idx].try_lock()) {
      unlocker->SetLock(&lock_pool_[lock_idx]);
      return true;
    }
    return false;
  }

  // Unlock.
  inline void Unlock(K idx) {
    int lock_idx = GetLockIndex(idx);
    lock_pool_[lock_idx].unlock();
  }

private:
  int GetLockIndex(K idx) {
    //    return hasher(idx) % lock_pool_size_;
    return int(idx) % lock_pool_size_;
  }

  // Use default HASH function.
  static HASH hasher;

  // Size of the pool. Currently does not support resizing.
  const int32_t lock_pool_size_;

  // pool of mutexes.
  boost::scoped_array<MUTEX> lock_pool_;
};

}  // namespace petuum
