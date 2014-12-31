#ifndef PETUUM_LRU_BUFFER
#define PETUUM_LRU_BUFFER


#include "petuum_ps/storage/lru_eviction_logic.hpp"
#include <cstdlib>
#include <queue>
#include <mutex>

extern "C"


namespace petuum
{
  // This is an implementation of multiple buffering.
  // This implementation allows for reader continue to read while
  // the writer is still updating the lru_eviction_logic.
  class MultiBuffer
  {
    public:

      struct Buffer
      {
        std::queue<EvictionLogic::list_iter_t>* request_queue_;
      };

      // constructor
      MultiBuffer(int max_number_buffer, int drain_threshold);
      // destructor
      ~MultiBuffer();



      // grab the lock and lookup the current buffer
      // then add the request to the buffer
      // then check the size of the buffer to see
      // if there reached to max
      // switch to the next buffer and drain the old buffer
      void add_to_buffer(EvictionLogic::list_iter_t target);
      // drain the buffer and update the eviction logic
      // should already changed to next buffer
      void drain_buffer();
      void drain_buffer(unsigned int to_be_drained);
      int32_t evict_drain_buffer();
      // switch to the next buffer
      void next_buffer();

      // wrapper for the eviction logic
      EvictionLogic::list_iter_t put(int32_t row_id);

      EvictionLogic* eviction_logic_;

    private:
      // this holds multiple buffer
      struct Buffer** multi_buffer_;
      // max number of buffer
      unsigned int max_num_buffer_;
      // current buffer index
      unsigned int current_buff_index_;

      unsigned int drain_threshold_;


      // lock it when thread want to add to buffer
      std::mutex add_to_buffer_lock_;
      // for draining
      std::mutex drain_lock_;
  }; // class MultiBuffer

  MultiBuffer::MultiBuffer(int max_number_buffer, int drain_threshold)
  {
    multi_buffer_ = new struct Buffer* [max_number_buffer];
    for (int i = 0; i < max_number_buffer; i++)
    {
      multi_buffer_[i] = new struct Buffer;
      multi_buffer_[i]->request_queue_ = new std::queue<EvictionLogic::list_iter_t>;
    }
    max_num_buffer_ = max_number_buffer;
    current_buff_index_ = 0;
    drain_threshold_ = drain_threshold;
    eviction_logic_ = new EvictionLogic(100,1);
  }

  MultiBuffer::~MultiBuffer()
  {
    delete eviction_logic_;
    for (int i = 0; i < max_num_buffer_; i++)
    {
      delete (multi_buffer_[i]->request_queue_);
      delete (multi_buffer_[i]);
    }
    delete multi_buffer_;
  }

  void MultiBuffer::add_to_buffer(EvictionLogic::list_iter_t target)
  {
    // lock the buffers up
    add_to_buffer_lock_.lock();
    // find the current buffer
    struct Buffer* current_buffer =
      multi_buffer_[current_buff_index_];
    // add the request to buffer
    current_buffer->request_queue_->push(target);
    // check if need to drain buffer
    if (current_buffer->request_queue_->size() > drain_threshold_ &&
        drain_lock_.try_lock())
    {
      unsigned int old_buffer = current_buff_index_;
      next_buffer();
      add_to_buffer_lock_.unlock();
      drain_buffer(old_buffer);
      drain_lock_.unlock();
      return;
    }
    // unlock
    add_to_buffer_lock_.unlock();
    return;
  }

  void MultiBuffer::next_buffer()
  {
    current_buff_index_ = (current_buff_index_ + 1) % max_num_buffer_;
    return;
  }

  void MultiBuffer::drain_buffer(unsigned int to_be_drained)
  {
    struct Buffer* draining_buf = multi_buffer_[to_be_drained];
    while(!draining_buf->request_queue_->empty())
    {
      EvictionLogic::list_iter_t target = draining_buf->request_queue_->front();
      eviction_logic_->Touch(target);
      draining_buf->request_queue_->pop();
    }
    return;
  }

  void MultiBuffer::drain_buffer()
  {
    add_to_buffer_lock_.lock();
    drain_lock_.lock();
    this->drain_buffer(this->current_buff_index_);
    drain_lock_.unlock();
    add_to_buffer_lock_.unlock();
    return;
  }

  int32_t MultiBuffer::evict_drain_buffer()
  {
    add_to_buffer_lock_.lock();
    drain_lock_.lock();
    this->drain_buffer(this->current_buff_index_);
    int32_t row_id = eviction_logic_->Get_evict();
    drain_lock_.unlock();
    add_to_buffer_lock_.unlock();
    return row_id;
  }

  EvictionLogic::list_iter_t MultiBuffer::put(int32_t row_id)
  {
    return eviction_logic_->Put(row_id);
  }

}// name space


#endif
