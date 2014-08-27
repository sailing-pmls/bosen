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
#include <petuum_ps_common/util/lock.hpp>
#include <cassert>
#include <boost/thread.hpp>   // pthread status code
#include <pthread.h>

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
