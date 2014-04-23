// Author: jinliang
#pragma once

#include <stdint.h>
#include <boost/unordered_map.hpp>

#include "petuum_ps/include/abstract_row.hpp"

namespace petuum {

class RowOpLog : boost::noncopyable {
public:
  RowOpLog(uint32_t update_size, const AbstractRow *sample_row):
    update_size_(update_size),
    sample_row_(sample_row) { }

  ~RowOpLog() {
    boost::unordered_map<int32_t, void*>::iterator iter = oplogs_.begin();
    for (; iter != oplogs_.end(); iter++) {
      delete reinterpret_cast<uint8_t*>(iter->second);
    }
  }

  void* Find(int32_t col_id) {
    boost::unordered_map<int32_t, void*>::iterator iter
      = oplogs_.find(col_id);
    if (iter == oplogs_.end()) {
      return 0;
    }
    return iter->second;
  }

  void* FindCreate(int32_t col_id) {
    boost::unordered_map<int32_t, void*>::iterator iter
      = oplogs_.find(col_id);
    if (iter == oplogs_.end()) {
      void* update = reinterpret_cast<void*>(new uint8_t[update_size_]);
      sample_row_->InitUpdate(col_id, update);
      oplogs_[col_id] = update;
      return update;
    }
    return iter->second;
  }

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

  int32_t GetSize(){
    return oplogs_.size();
  }

private:
  uint32_t update_size_;
  boost::unordered_map<int32_t, void*> oplogs_;
  const AbstractRow *sample_row_;
  boost::unordered_map<int32_t, void*>::iterator iter_;
};
}
