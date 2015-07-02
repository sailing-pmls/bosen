#include <petuum_ps_common/util/lock.hpp>
#include <cassert>
#include <boost/thread.hpp>   // pthread status code
#include <pthread.h>
#include <glog/logging.h>

namespace petuum {

// ==================== SharedMutex ===================

SharedMutex::SharedMutex() {
  int status = pthread_rwlock_init(&rw_lock_, NULL);
  assert(status == 0);
}

SharedMutex::~SharedMutex() {
  int status = pthread_rwlock_destroy(&rw_lock_);
  assert(status == 0);
}

void SharedMutex::lock() {
  int status = pthread_rwlock_wrlock(&rw_lock_);
  CHECK_EQ(status, 0);
  assert(status == 0);
}

bool SharedMutex::try_lock() {
  int status = pthread_rwlock_trywrlock(&rw_lock_);

  switch (status) {
    case 0:
      return true;

    case EBUSY:
      return false;

    case EDEADLK:
    default:
      assert(false);
  }
}

// glibc seems to be buggy ... don't unlock more often
// than it has been locked
// http://sourceware.org/bugzilla/show_bug.cgi?id=4825
void SharedMutex::unlock() {
  int status = pthread_rwlock_unlock(&rw_lock_);
  assert(status == 0);
}

void SharedMutex::lock_shared() {
  int status = pthread_rwlock_rdlock(&rw_lock_);
  assert(status == 0);
}

bool SharedMutex::try_lock_shared() {
  int status = pthread_rwlock_tryrdlock(&rw_lock_);

  if (status == 0)
    return true;
  if (status == EBUSY)
    return false;

  assert(false);
}

void SharedMutex::unlock_shared() {
  unlock();
}

// ==================== RecursiveSharedMutex ===================

RecursiveSharedMutex::RecursiveSharedMutex()
  : write_lock_count_(0), writer_id(0) { }

RecursiveSharedMutex::~RecursiveSharedMutex() {
  assert(writer_id == 0);
}

void RecursiveSharedMutex::lock() {
  int status = pthread_rwlock_wrlock(&rw_lock_);

  switch (status) {
    case 0:
      writer_id = pthread_self();
      assert(write_lock_count_ == 0);

    case EDEADLK:
      assert(writer_id == pthread_self());
      ++write_lock_count_;
      return;

    default:
      assert(false);
  }
}

bool RecursiveSharedMutex::try_lock() {
  int status = pthread_rwlock_trywrlock(&rw_lock_);

  switch (status) {
    case 0:
      assert(writer_id == 0);
      assert(write_lock_count_ == 0);
      writer_id = pthread_self();
      ++write_lock_count_;
      return true;

    case EDEADLK:
      assert(writer_id == pthread_self());

    case EBUSY:
      if (writer_id == pthread_self())
      {
        assert(write_lock_count_ > 0);
        ++write_lock_count_;
        return true;
      }
      else
      {
        assert(writer_id != pthread_self());
        return false;
      }

    default:
      assert(false);
  }
}

void RecursiveSharedMutex::unlock() {
  assert(write_lock_count_ > 0);
  assert(writer_id);
  if (--write_lock_count_ == 0) {
    writer_id = 0;
    SharedMutex::unlock();
  }
}

void RecursiveSharedMutex::lock_shared() {
  if (writer_id == pthread_self())
    // we're already owning it as writer
    return;
  SharedMutex::lock_shared();
}

bool RecursiveSharedMutex::try_lock_shared() {
  if (writer_id == pthread_self())
    // we're already owning it as writer
    return true;

  return SharedMutex::try_lock_shared();
}

void RecursiveSharedMutex::unlock_shared() {
  if (writer_id == pthread_self())
    // we're already owning it as writer
    return;

  int status = pthread_rwlock_unlock(&rw_lock_);
  assert(status == 0);
}

}   // namespace petuum
