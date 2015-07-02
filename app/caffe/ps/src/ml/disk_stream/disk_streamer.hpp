// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.11.22

#pragma once

#include <string>
#include <cstdint>
#include <thread>
#include <vector>
#include <mutex>
#include <glog/logging.h>
#include <ml/disk_stream/multi_buffer.hpp>
#include <ml/disk_stream/disk_reader.hpp>
#include <ml/disk_stream/parsers/abstract_parser.hpp>
#include <boost/thread/barrier.hpp>

#include <map>

namespace petuum {
namespace ml {

struct DiskStreamerConfig {
  int32_t num_buffers = 2;          // # of buffers in total to rotate.
  int32_t num_worker_threads = -1;   // not counting IO thread.

  DiskReaderConfig disk_reader_config;
};

template<typename DATUM>
class DiskStreamer {
public:
  // DiskStreamer takes ownership of parser.
  DiskStreamer(const DiskStreamerConfig& config,
      AbstractParser<DATUM>* parser);

  // Destructor waits for disk_reader_thread_ to join.
  ~DiskStreamer();

  // Attempt to parse 'num_data' data.  Return 0 or more (up to 'num_data')
  // data. Returning 0 datum signals end of stream, and calling any function
  // in DiskStreamer afterwards result in undefined behavior. Thread-safe.
  std::vector<DATUM*> GetNextData(int num_data);

  // Stop the disk reading thread if it's running. Calling it multiple times
  // is the same as calling it once. After this call a thread cannot call
  // any DiskStreamer function anymore. Thread-safe.
  void Shutdown();

private:  // private functions
  std::vector<DATUM*> ParseData(int num_data);

private:
  std::thread disk_reader_thread_;

  MultiBuffer multi_buffer_;

  DiskReader disk_reader_;

  std::unique_ptr<AbstractParser<DATUM> > parser_;

  std::mutex mtx_;

  ByteBuffer* curr_work_buffer_;

  boost::barrier barrier_;

  bool buffer_active_;
};

// ================ Implementation =================

template<typename DATUM>
DiskStreamer<DATUM>::DiskStreamer(const DiskStreamerConfig& config,
    AbstractParser<DATUM>* parser) :
  multi_buffer_(config.num_buffers),
  disk_reader_(config.disk_reader_config, &multi_buffer_),
  parser_(parser), barrier_(config.num_worker_threads) {
    CHECK_NE(-1, config.num_worker_threads);
    CHECK(parser_) << "Data parser cannot be null.";
    disk_reader_thread_ = std::thread(&DiskReader::Start,
        std::ref(disk_reader_));
    // Get work buffer after the reader thread starts.
    curr_work_buffer_ = multi_buffer_.GetWorkBuffer();
    if (curr_work_buffer_ == 0) {
      LOG(FATAL) << "IO thread shutdown before reading anything!";
    }
    buffer_active_ = true;
  }

template<typename DATUM>
DiskStreamer<DATUM>::~DiskStreamer() {
  disk_reader_thread_.join();
  LOG(INFO) << "disk_reader_thread_ joined.";
}

template<typename DATUM>
std::vector<DATUM*> DiskStreamer<DATUM>::GetNextData(int num_data) {
  while (true) {
    {
      std::unique_lock<std::mutex> lock(mtx_);
      if (curr_work_buffer_ == 0) {
        return std::vector<DATUM*>(0);   // end of data stream.
      }
      if (curr_work_buffer_->HasMore()) {
        return ParseData(num_data);
      }
      buffer_active_ = false;
    }

    barrier_.wait();
    {
      std::unique_lock<std::mutex> lock(mtx_);
      if (!buffer_active_) {
        // Replace current buffer.
        multi_buffer_.DoneConsumingWorkBuffer();
        curr_work_buffer_ = multi_buffer_.GetWorkBuffer();
        buffer_active_ = true;
      }
    }

    // Without this barrier, thread 1 can replace curr_work_buffer_ with 0
    // (null) and thread 2 immediately get (curr_work_buffer_ == 0) and
    // return, leaving thread 1 waiting at the first barrier_ (deadlock).
    barrier_.wait();
  }
}

template<typename DATUM>
std::vector<DATUM*> DiskStreamer<DATUM>::ParseData(int num_data) {
  char* data_ptr = curr_work_buffer_->GetNextBytes();
  std::vector<DATUM*> parsed_data;
  for (int i = 0; i < num_data && curr_work_buffer_->HasMore();
      ++i) {
    int num_bytes = 0;
    parsed_data.push_back(parser_->Parse(data_ptr, &num_bytes));
    curr_work_buffer_->AdvanceOffset(num_bytes);
    data_ptr = curr_work_buffer_->GetNextBytes();
  }
  CHECK_GT(parsed_data.size(), 0)
    << "Bytes in buffer do not form a complete datum.";
  return parsed_data;
}

template<typename DATUM>
void DiskStreamer<DATUM>::Shutdown() {
  multi_buffer_.WorkerThreadShutdown();
  while (GetNextData(1).size() > 0);
}

}  // namespace ml
}  // namespace petuum
