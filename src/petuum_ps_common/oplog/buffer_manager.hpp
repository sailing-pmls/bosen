// Author: Jinliang

#pragma once

#include <petuum_ps_common/oplog/abstract_append_only_buffer.hpp>
#include <petuum_ps_common/oplog/dense_append_only_buffer.hpp>
#include <petuum_ps_common/oplog/inc_append_only_buffer.hpp>
#include <petuum_ps_common/oplog/batch_inc_append_only_buffer.hpp>

#include <boost/noncopyable.hpp>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <iostream>

namespace petuum {

class BufferPool : boost::noncopyable {
public:
  BufferPool(int32_t thread_id, size_t pool_size, AppendOnlyOpLogType append_only_oplog_type,
             size_t buff_capacity, size_t update_size, size_t dense_row_capacity):
      pool_size_(pool_size),
      num_buffs_in_pool_(pool_size),
      begin_(0),
      end_(0),
      pool_(pool_size) {
    for (auto &buff_ptr : pool_) {
      buff_ptr = CreateAppendOnlyBuffer(thread_id,
                                        append_only_oplog_type, buff_capacity,
                                        update_size, dense_row_capacity);
    }
  }

  ~BufferPool() {
    for (auto &buff_ptr : pool_) {
      delete buff_ptr;
      buff_ptr = 0;
    }
  }

  AbstractAppendOnlyBuffer *GetBuff() {
    std::unique_lock<std::mutex> lock(mtx_);
    while (num_buffs_in_pool_ == 0) cv_.wait(lock);

    num_buffs_in_pool_--;
    AbstractAppendOnlyBuffer *buff = pool_[begin_];
    pool_[begin_] = 0;
    begin_ = (begin_ + 1) % pool_size_;
    return buff;
  }

  void PutBuff(AbstractAppendOnlyBuffer *buff) {
    std::unique_lock<std::mutex> lock(mtx_);
    CHECK(num_buffs_in_pool_ < pool_size_) << "buffer pool is full!";

    pool_[end_] = buff;
    end_ = (end_ + 1) % pool_size_;
    num_buffs_in_pool_++;

    if (num_buffs_in_pool_ == 1)
      cv_.notify_all();
  }

private:
  static AbstractAppendOnlyBuffer *CreateAppendOnlyBuffer(
      int32_t thread_id,
      AppendOnlyOpLogType append_only_oplog_type,
      size_t buff_capacity, size_t update_size, size_t dense_row_capacity) {
    AbstractAppendOnlyBuffer *buff = 0;
    switch(append_only_oplog_type) {
      case Inc:
        buff = static_cast<AbstractAppendOnlyBuffer*>(
            new IncAppendOnlyBuffer(thread_id, buff_capacity, update_size));
        break;
      case BatchInc:
        buff = static_cast<AbstractAppendOnlyBuffer*>(
            new BatchIncAppendOnlyBuffer(thread_id, buff_capacity, update_size));
        break;
      case DenseBatchInc:
        buff = static_cast<AbstractAppendOnlyBuffer*>(
            new DenseAppendOnlyBuffer(thread_id, buff_capacity, update_size,
                                      dense_row_capacity));
      break;
      default:
        LOG(FATAL) << "Unknown type = " << append_only_oplog_type;
    }
    return buff;
  }

  const size_t pool_size_;

  size_t num_buffs_in_pool_;
  int begin_, end_;
  std::vector<AbstractAppendOnlyBuffer*> pool_;
  std::mutex mtx_;
  std::condition_variable cv_;
};

class OpLogBufferManager {
public:
  OpLogBufferManager(size_t num_table_threads, int32_t head_thread_id):
      head_thread_id_(head_thread_id),
      buffer_pool_vec_(num_table_threads) { }

  ~OpLogBufferManager() {
    for (auto &pool_ptr : buffer_pool_vec_) {
      delete pool_ptr;
      pool_ptr = 0;
    }
  }

  void CreateBufferPool(
      int32_t thread_id,  size_t pool_size, AppendOnlyOpLogType append_only_oplog_type,
      size_t buff_capacity, size_t update_size, size_t dense_row_capacity) {
    int32_t idx = thread_id - head_thread_id_;
    buffer_pool_vec_[idx] = new BufferPool(
        thread_id, pool_size, append_only_oplog_type, buff_capacity, update_size,
        dense_row_capacity);
  }

  AbstractAppendOnlyBuffer *GetBuff(int32_t thread_id) {
    int32_t idx = thread_id - head_thread_id_;
    return buffer_pool_vec_[idx]->GetBuff();
  }

  void PutBuff(int32_t thread_id, AbstractAppendOnlyBuffer *buff) {
    int32_t idx = thread_id - head_thread_id_;
    buffer_pool_vec_[idx]->PutBuff(buff);
  }

private:
  const int32_t head_thread_id_;
  std::vector<BufferPool*> buffer_pool_vec_;
};

}
