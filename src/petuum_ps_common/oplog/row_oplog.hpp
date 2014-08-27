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
// Author: jinliang
#pragma once

#include <stdint.h>
#include <map>

#include <functional>

namespace petuum {

typedef std::function<void(int32_t, void *)> InitUpdateFunc;

class RowOpLog : boost::noncopyable {
public:
  RowOpLog(uint32_t update_size, InitUpdateFunc InitUpdate):
    update_size_(update_size),
    InitUpdate_(InitUpdate) { }

  ~RowOpLog() {
    auto iter = oplogs_.begin();
    for (; iter != oplogs_.end(); iter++) {
      delete reinterpret_cast<uint8_t*>(iter->second);
    }
  }

  void* Find(int32_t col_id) {
    auto iter = oplogs_.find(col_id);
    if (iter == oplogs_.end()) {
      return 0;
    }
    return iter->second;
  }

  const void* FindConst(int32_t col_id) const {
    auto iter = oplogs_.find(col_id);
    if (iter == oplogs_.end()) {
      return 0;
    }
    return iter->second;
  }

  void* FindCreate(int32_t col_id) {
    auto iter = oplogs_.find(col_id);
    if (iter == oplogs_.end()) {
      void* update = reinterpret_cast<void*>(new uint8_t[update_size_]);
      InitUpdate_(col_id, update);
      oplogs_[col_id] = update;
      return update;
    }
    return iter->second;
  }

  // Guaranteed ordered traversal
  void* BeginIterate(int32_t *column_id) {
    iter_ = oplogs_.begin();
    if (iter_ == oplogs_.end()) {
      return 0;
    }
    *column_id = iter_->first;
    return iter_->second;
  }

  void* Next(int32_t *column_id) {
    iter_++;
    if (iter_ == oplogs_.end()) {
      return 0;
    }
    *column_id = iter_->first;
    return iter_->second;
  }

  // Guaranteed ordered traversal, in ascending order of column_id
  const void* BeginIterateConst(int32_t *column_id) const {
    const_iter_ = oplogs_.cbegin();
    if (const_iter_ == oplogs_.cend()) {
      return 0;
    }
    *column_id = const_iter_->first;
    return const_iter_->second;
  }

  const void* NextConst(int32_t *column_id) const {
    const_iter_++;
    if (const_iter_ == oplogs_.cend()) {
      return 0;
    }
    *column_id = const_iter_->first;
    return const_iter_->second;
  }

  int32_t GetSize() const {
    return oplogs_.size();
  }

private:
  const uint32_t update_size_;
  std::map<int32_t, void*> oplogs_;
  InitUpdateFunc InitUpdate_;
  std::map<int32_t, void*>::iterator iter_;
  mutable std::map<int32_t, void*>::const_iterator const_iter_;
};
}
