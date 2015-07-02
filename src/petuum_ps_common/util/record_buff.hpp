#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <boost/noncopyable.hpp>
#include <glog/logging.h>

namespace petuum {

// A buffer that allows appending records to it.
class RecordBuff : boost::noncopyable {
public:
  RecordBuff() {
    LOG(FATAL) << "Default constructor called";
  }
  RecordBuff(void *mem, size_t size):
      mem_(reinterpret_cast<uint8_t*>(mem)),
      mem_size_(size),
      offset_(0) { }
  ~RecordBuff() { }

  RecordBuff(RecordBuff && other):
      mem_(other.mem_),
      mem_size_(other.mem_size_),
      offset_(other.offset_) {
    //VLOG(0) << "mem_size_ = " << mem_size_;
  };

  // does not take ownership of the memory
  void *ResetMem(void *mem, size_t size) {
    void *old_mem = mem_;
    mem_ = reinterpret_cast<uint8_t*>(mem);
    mem_size_ = size;
    offset_ = 0;
    return old_mem;
  }

  void ResetOffset() {
    offset_ = 0;
  }

  bool Append(int32_t record_id, const void *record, size_t record_size) {
    if (offset_ + sizeof(int32_t) + record_size + sizeof(size_t) > mem_size_) {
      return false;
    }
    *(reinterpret_cast<int32_t*>(mem_ + offset_)) = record_id;
    offset_ += sizeof(int32_t);
    *(reinterpret_cast<size_t*>(mem_ + offset_)) = record_size;
    offset_ += sizeof(size_t);
    memcpy(mem_ + offset_, record, record_size);
    offset_ += record_size;
    //VLOG(0) << "Append() end offset = " << offset_;
    return true;
  }

  size_t GetMemUsedSize() {
    return offset_;
  }

  int32_t *GetMemPtrInt32() {
    if (offset_ + sizeof(int32_t) > mem_size_) {
      return 0;
    }
    int32_t *ret_ptr = reinterpret_cast<int32_t*>(mem_ + offset_);
    offset_ += sizeof(int32_t);

    return ret_ptr;
  }

  void PrintInfo() const {
    VLOG(0) << "mem_ = " << mem_
            << " mem_size_ = " << mem_size_
            << " offset = " << offset_;
  }

private:
  uint8_t *mem_;
  size_t mem_size_;
  size_t offset_;
};


}  // namespace petuum
