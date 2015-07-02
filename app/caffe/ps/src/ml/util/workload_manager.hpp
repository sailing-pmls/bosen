// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.07.14

#pragma once

#include <glog/logging.h>
#include <cstdint>
#include <cmath>

namespace petuum {
namespace ml {

struct WorkloadManagerConfig {
  int32_t thread_id;
  int32_t client_id;
  int32_t num_clients;
  int32_t num_threads;
  int32_t num_batches_per_epoch;
  int32_t num_data;
  bool global_data;  // true if dataset is duplicated on other clients.
};

class WorkloadManager {
public:
  WorkloadManager(const WorkloadManagerConfig& config) {
    int32_t thread_id = config.thread_id;
    int32_t client_id = config.client_id;
    int32_t num_clients = config.num_clients;
    int32_t num_threads = config.num_threads;
    int32_t num_batches_per_epoch = config.num_batches_per_epoch;
    bool global_data = config.global_data;

    // Each thread handles data [data_idx_begin_, data_idx_end_).
    int num_data = config.num_data;
    int num_data_per_thread;
    if (global_data) {
      size_t num_total_threads = num_clients * num_threads;
      num_data_per_thread = (num_data + num_total_threads - 1) / num_total_threads;

      size_t total_data_assigned = num_data_per_thread * num_total_threads;
      int32_t cutoff_id = num_total_threads - 1 - (total_data_assigned - num_data);

      int32_t my_id = client_id *num_threads + thread_id;

      if (my_id <= cutoff_id) {
        data_idx_begin_ = num_data_per_thread * my_id;
        data_idx_end_ = data_idx_begin_ + num_data_per_thread;
      } else if (my_id == cutoff_id + 1) {
        data_idx_begin_ = num_data_per_thread * my_id;
        data_idx_end_ = data_idx_begin_ + num_data_per_thread - 1;
      } else {
        data_idx_begin_ = num_data_per_thread * cutoff_id
                          + (num_data_per_thread - 1) * (my_id - cutoff_id);
        data_idx_end_ = data_idx_begin_ + num_data_per_thread - 1;
      }

    } else {
      num_data_per_thread = num_data / num_threads;
      data_idx_begin_ = num_data_per_thread * thread_id;
      // The last thread takes the rest of the data.
      if (thread_id == num_threads - 1) {
        data_idx_end_ = num_data;
      } else {
        data_idx_end_ = data_idx_begin_ + num_data_per_thread;
      }
    }

    // We will allow wrap-around on [data_idx_begin_, data_idx_end_)  when
    // data points aren't divisible.
    batch_size_ = std::ceil(
        static_cast<float>(data_idx_end_ - data_idx_begin_)
        / num_batches_per_epoch);
    CHECK_LT(0, batch_size_)
        << "Batch size cannot be 0. # data this thread: "
        << (data_idx_end_ - data_idx_begin_)
        << " num_batches_per_epoch: "
        << num_batches_per_epoch;
    num_data_per_epoch_ = batch_size_ * num_batches_per_epoch;
    Restart();
  }

  int32_t GetBatchSize() const {
    return batch_size_;
  }

  int32_t GetNumBatches() const {
    // This must be divisible from constructor's construction.
    return num_data_per_epoch_ / batch_size_;
  }

  void Restart() {
    num_data_this_epoch_ = 0;
  }

  // Get a data index and advance.
  int32_t GetDataIdxAndAdvance() {
    CHECK(!IsEnd());
    int32_t ret_idx = num_data_this_epoch_++ + data_idx_begin_;
    return WrapAround(ret_idx);
  }

  // Get the next num_data indices without advancing.
  std::vector<int32_t> GetBatchDataIdx(int32_t num_data) const {
    std::vector<int32_t> result(num_data);
    for (int i = 0; i < num_data; ++i) {
      result[i] = (num_data_this_epoch_ + data_idx_begin_ + i);
      // Wrap around to be within [data_idx_begin_, data_idx_end_)
      result[i] = WrapAround(result[i]);
    }
    return result;
  }

  // Is end of the data set (of this partition).
  bool IsEnd() const {
    return num_data_this_epoch_ == num_data_per_epoch_;
  }

  bool IsEndOfBatch() const {
    return num_data_this_epoch_ % batch_size_ == 0;
  }
private:
  // Wrap around idx to within [data_idx_begin_, data_idx_end_).
  int32_t WrapAround(int32_t idx) const {
    return (idx >= data_idx_end_) ?
      (idx - data_idx_end_) % (data_idx_end_ - data_idx_begin_)
      + data_idx_begin_ : idx;
  }

private:
  // A partition's index range is [data_idx_begin_, data_idx_end_).
  int32_t data_idx_begin_;
  int32_t data_idx_end_;

  // # of data points to go through before IsEndOfBatch() returns true.
  int32_t batch_size_;
  int32_t num_data_this_epoch_;
  int32_t num_data_per_epoch_;
};


}  // namespace ml
}  // namespace petuum
