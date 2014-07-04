// Copyright (c) 2014, Sailing Lab
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the <ORGANIZATION> nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
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
    //VLOG(0) << "Append() offset_ = " << offset_
    //      << " record_size = " << record_size
    //      << " mem_size_ = " << mem_size_;
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
      VLOG(0) << "Exceeded! GetMemPtrInt32() offset_ = " << offset_
              << " mem_size_ = " << mem_size_;
      return 0;
    }
    int32_t *ret_ptr = reinterpret_cast<int32_t*>(mem_ + offset_);
    offset_ += sizeof(int32_t);
    //VLOG(0) << "GetMemPtrInt32() offset_ = " << offset_;
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
