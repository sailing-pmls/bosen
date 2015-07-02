// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.11.22

#include <string>
#include <cstdint>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <glog/logging.h>
#include <ml/disk_stream/multi_buffer.hpp>
#include <ml/disk_stream/byte_buffer.hpp>

namespace petuum {
namespace ml {

MultiBuffer::MultiBuffer(int32_t num_buffers) : io_thread_shutdown_(false),
  worker_thread_shutdown_(false) {
  // Create num_buffers in free_buffers_.
  for (int i = 0; i < num_buffers; ++i) {
    free_buffers_.push(new ByteBuffer);
  }
}

ByteBuffer* MultiBuffer::GetWorkBuffer() {
  std::unique_lock<std::mutex> lock(mtx_);
  work_buffer_not_empty_.wait(lock, [&]() {
      return work_buffers_.size() > 0 || io_thread_shutdown_; });
  if ((io_thread_shutdown_ && work_buffers_.size() == 0) ||
      worker_thread_shutdown_) {
    // Exit after all work buffers are consumed since disk reader thread
    // could exit after one sweep but worker threads need to see all read
    // data.
    return 0;   // signal end of stream.
  }
  return work_buffers_.front();
}

void MultiBuffer::DoneConsumingWorkBuffer() {
  std::unique_lock<std::mutex> lock(mtx_);
  // Move outstanding worker buffer to free_buffers_.
  free_buffers_.push(work_buffers_.front());
  work_buffers_.pop();
  free_buffers_not_empty_.notify_one();
}

ByteBuffer* MultiBuffer::GetIOBuffer() {
  std::unique_lock<std::mutex> lock(mtx_);
  free_buffers_not_empty_.wait(lock,
      [&](){ return free_buffers_.size() > 0 || worker_thread_shutdown_; });
  if (worker_thread_shutdown_) {
    return 0;  // shutdown message to the IO thread.
  }
  return free_buffers_.front();
}

void MultiBuffer::DoneFillingIOBuffer() {
  std::unique_lock<std::mutex> lock(mtx_);
  // Move the filled buffer to work_buffers_.
  work_buffers_.push(free_buffers_.front());
  free_buffers_.pop();
  work_buffer_not_empty_.notify_one();
}

void MultiBuffer::IOThreadShutdown() {
  io_thread_shutdown_ = true;
  work_buffer_not_empty_.notify_one();
}

void MultiBuffer::WorkerThreadShutdown() {
  worker_thread_shutdown_ = true;
  free_buffers_not_empty_.notify_one();
}

}  // namespace ml
}  // namespace petuum
