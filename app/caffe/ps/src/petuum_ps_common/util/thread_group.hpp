#pragma once

#include <vector>

namespace petuum {

template <typename ThreadType>
class ThreadGroup {
public:
  virtual ~ThreadGroup() {
    for (auto &thread : threads_) {
      delete thread;
    }
  }

  void StartAll() {
    for (auto &thread : threads_) {
      thread->Start();
    }
  }

  void JoinAll() {
    for (auto &thread : threads_) {
      thread->Join();
    }
  }

protected:
    ThreadGroup(size_t num_threads):
      threads_(num_threads) { }

  std::vector<ThreadType*> threads_;
};
}
