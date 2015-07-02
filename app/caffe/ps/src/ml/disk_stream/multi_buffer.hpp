// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.11.22

#pragma once

#include <string>
#include <cstdint>
#include <queue>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <boost/utility.hpp>
#include <ml/disk_stream/byte_buffer.hpp>

namespace petuum {
namespace ml {

// MultiBuffer supports 1 IO thread to read and 1 reader thread sharing
// multiple buffers.
class MultiBuffer : boost::noncopyable {
public:
  MultiBuffer(int32_t num_buffers);

  // Get the work buffer (i.e., buffer ready for workers threads to consume).
  // Block if no buffer is ready. Only one worker thread should call this
  // method.
  ByteBuffer* GetWorkBuffer();

  // Worker calls this to signal the buffer obtained from GetWorkBuffer() is
  // fully consumed by worker threads. Only one worker thread should call this
  // method.
  void DoneConsumingWorkBuffer();

  // Get a buffer that is either (1) unused or (2) worker threads have
  // finished consuming the content. Return 0 (null) to signal to IO thread to
  // shutdown.  Only IO thread should call this method.
  ByteBuffer* GetIOBuffer();

  // IO thread pushes a buffer from free_buffers_ to work_buffers_.
  void DoneFillingIOBuffer();

  // Release the worker thread (if it's blocked). Worker will finish
  // consuming work_buffers_ and get no more. Can only be called by io
  // thread, or behavior is undefined. Calling it multiple times is the same
  // as calling it once (idempotent).
  void IOThreadShutdown();

  // Release io thread (if it is blocked) and send shutdown signal to it. Can
  // only be called by worker thread, or behavior is undefined. Calling it
  // multiple times is the same as calling it once (idempotent).
  void WorkerThreadShutdown();

private:
  // Buffers available for IO thread to fill.
  std::queue<ByteBuffer*> free_buffers_;

  // Buffers ready for the worker thread to consume.
  std::queue<ByteBuffer*> work_buffers_;

  // mtx_ (1) synchronizes access to free_buffers_ and work_buffers_; (2)
  // mutex for work_buffer_not_empty_ and free_buffers_not_empty_.
  std::mutex mtx_;

  // Worker thread waits on work_buffer_not_empty_ if no work buffer is
  // ready (i.e., no buffer is supplied by IO thread).
  std::condition_variable work_buffer_not_empty_;

  // IO thread waits on free_buffers_not_empty_ if free_buffers_.empyt().
  std::condition_variable free_buffers_not_empty_;

  // Shutdown signal variable.
  std::atomic_bool io_thread_shutdown_;
  std::atomic_bool worker_thread_shutdown_;
};

}  // namespace ml
}  // namespace petuum
