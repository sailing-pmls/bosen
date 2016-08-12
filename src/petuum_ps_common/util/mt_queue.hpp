// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.01.31

#pragma once

#include <queue>
#include <mutex>

namespace petuum {

/**
 * Wrap around std::queue and provide thread safety (MT = multi-threaded).
 */
template<typename T>
class MTQueue {
  public:
    MTQueue() { }

    /**
     * Return 0 if it's empty, 1 if not. val is unset if returning 0. Note the
     * different semantics than std::queue::pop.
     */
    int pop(T* val) {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!q_.empty()) {
        *val = q_.front();
        q_.pop();
        return 1;
      }
      return 0;
    }

    void push(const T& val) {
      std::lock_guard<std::mutex> lock(mutex_);
      q_.push(val);
    }

  private:
    std::queue<T> q_;
    std::mutex mutex_;
};


}   // namespace petuum
