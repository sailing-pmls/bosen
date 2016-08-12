// Author: Aurick Qiao (aqiao@cs.cmu,edu), Jinliang
// Date: 2014.10.06

#pragma once

#include <cassert>
#include <cstring>
#include <condition_variable>
#include <mutex>

#include <memory>
#include <boost/noncopyable.hpp>

namespace petuum {

/**
 * MPMCQueue is a multi-producer-multi-consumer bounded buffer.
 */

template<typename T>
class MPMCQueue : boost::noncopyable {
public:
  MPMCQueue(size_t capacity)
  : capacity_(capacity),
    size_(0),
    buffer_(new T[capacity]) {
    begin_ = buffer_.get();
    end_ = begin_;
  }

  ~MPMCQueue() { }

  size_t get_size() const {
    return size_;
  }

  void Push(const T& value) {
    std::unique_lock<std::mutex> lock(mtx_);
    while (size_ == capacity_) cv_.wait(lock);
    memcpy(end_, &value, sizeof(T));
    end_++;
    size_++;
    if (end_ == buffer_.get() + capacity_) end_ = buffer_.get();
  }

  bool Pop(T* value) {
    std::unique_lock<std::mutex> lock(mtx_);
    if (size_ == 0) return false;
    if (size_ == capacity_) cv_.notify_all();
    *value = *begin_;
    begin_++;
    size_--;
    if (begin_ == buffer_.get() + capacity_) begin_ = buffer_.get();
    return true;
  }

private:
  size_t capacity_, size_;
  std::unique_ptr<T[]> buffer_;
  T *begin_, *end_;
  std::mutex mtx_;
  std::condition_variable cv_;
};

}   // namespace petuum
