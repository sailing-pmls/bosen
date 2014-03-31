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
#include "petuum_ps/util/lock.hpp"
#include "petuum_ps/util/high_resolution_timer.hpp"
#include <boost/thread.hpp>
#include <glog/logging.h>
#include <gflags/gflags.h>
#include <mutex>
#include <unistd.h>   // sysconf

DEFINE_int32(count, 1e6, "number of read/write per thread.");
DEFINE_int32(num_threads, 0, "number of threads. 0 uses # of cores.");
DEFINE_int32(num_chunks, 4, "test write percentage at num_chunks + 1 points.");

// Shared dummy variable.
long dummy;

template<typename MUTEX>
struct Test {
  public:
    Test(long count, unsigned num_threads, unsigned write_ratio) :
      count_(count), num_threads_(num_threads), write_ratio_(write_ratio),
      value_(0) { }

    double run() {
      boost::thread_group g;
      petuum::HighResolutionTimer timer;

      for (int i = 1; i != num_threads_; ++i) {
        g.add_thread(new boost::thread(thf_fn, this));
      }

      thf();

      g.join_all();
      return timer.elapsed();
    }

  private:
    static void thf_fn(Test* this_) {
      this_->thf();
    }

    void thf() {
      int rand;

      for (long i = 0; i != count_; ++i) {
        rand = rand * 435898247 + 382842987;
        int num = rand % FLAGS_num_chunks;

        if (num < write_ratio_) {
          boost::lock_guard<MUTEX> lock(mutex_);
          dummy = value_;
        }
        else {
          boost::shared_lock<MUTEX> lock(mutex_);
          ++value_;
        }
      }
    };

    long value_;

    long count_;
    long num_threads_;
    long write_ratio_;

    MUTEX mutex_;
};

template<typename MUTEX>
struct TestMutex {
  public:
    TestMutex(long count, unsigned num_threads, unsigned write_ratio) :
      count_(count), num_threads_(num_threads), write_ratio_(write_ratio),
      value_(0) { }

    double run() {
      boost::thread_group g;
      petuum::HighResolutionTimer timer;

      for (int i = 1; i != num_threads_; ++i) {
        g.add_thread(new boost::thread(thf_fn, this));
      }

      thf();

      g.join_all();
      return timer.elapsed();
    }

  private:
    static void thf_fn(TestMutex* this_) {
      this_->thf();
    }

    void thf() {
      int rand;

      for (long i = 0; i != count_; ++i) {
        rand = rand * 435898247 + 382842987;
        int num = rand % FLAGS_num_chunks;

        if (num < write_ratio_) {
          boost::lock_guard<MUTEX> lock(mutex_);
          dummy = value_;
        }
        else {
          boost::lock_guard<MUTEX> lock(mutex_);
          ++value_;
        }
      }
    };

    long value_;

    long count_;
    long num_threads_;
    long write_ratio_;

    MUTEX mutex_;
};

template<typename MUTEX>
struct TestStdMutex {
  public:
    TestStdMutex(long count, unsigned num_threads, unsigned write_ratio) :
      count_(count), num_threads_(num_threads), write_ratio_(write_ratio),
      value_(0) { }

    double run() {
      boost::thread_group g;
      petuum::HighResolutionTimer timer;

      for (int i = 1; i != num_threads_; ++i) {
        g.add_thread(new boost::thread(thf_fn, this));
      }

      thf();

      g.join_all();
      return timer.elapsed();
    }

  private:
    static void thf_fn(TestStdMutex* this_) {
      this_->thf();
    }

    void thf() {
      int rand;

      for (long i = 0; i != count_; ++i) {
        rand = rand * 435898247 + 382842987;
        int num = rand % FLAGS_num_chunks;

        if (num < write_ratio_) {
          std::lock_guard<MUTEX> lock(mutex_);
          dummy = value_;
        }
        else {
          std::lock_guard<MUTEX> lock(mutex_);
          ++value_;
        }
      }
    };

    long value_;

    long count_;
    long num_threads_;
    long write_ratio_;

    MUTEX mutex_;
};

int main(int argc, char* argv[])
{
  // Initialize Google's logging library.
  google::InitGoogleLogging(argv[0]);

  long count = FLAGS_count;
  long num_threads = (FLAGS_num_threads == 0) ?
    sysconf(_SC_NPROCESSORS_ONLN) : FLAGS_num_threads;
  LOG(INFO) << "Using " << num_threads << " threads.";

  for (long write_ratio = 0; write_ratio != FLAGS_num_chunks + 1; ++write_ratio) {
    LOG(INFO) << "writes: "
      << static_cast<float>(write_ratio) / FLAGS_num_chunks * 100 << "%";
    {
      Test<boost::shared_mutex> tester(count, num_threads, write_ratio);
      LOG(INFO) << "boost::shared_mutex " << tester.run() << " seconds.";
    }
    {
      Test<petuum::SharedMutex> tester(count, num_threads, write_ratio);
      LOG(INFO) << "SharedMutex " << tester.run() << " seconds.";
    }
    {
      Test<petuum::RecursiveSharedMutex> tester(count, num_threads,
          write_ratio);
      LOG(INFO) << "RecursiveSharedMutex " << tester.run() << " seconds.";
    }
    {
      TestMutex<boost::mutex> tester(count, num_threads, write_ratio);
      LOG(INFO) << "boost::mutex " << tester.run() << " seconds.";
    }
    {
      TestStdMutex<std::mutex> tester(count, num_threads, write_ratio);
      LOG(INFO) << "std::mutex " << tester.run() << " seconds.";
    }
    {
      TestMutex<petuum::SpinMutex> tester(count, num_threads, write_ratio);
      LOG(INFO) << "SpinMutex " << tester.run() << " seconds.";
    }
    LOG(INFO) << "===============";
  }
}
