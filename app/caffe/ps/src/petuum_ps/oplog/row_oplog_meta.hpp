#pragma once

#include <stdint.h>

namespace petuum {

class RowOpLogMeta {
public:
  RowOpLogMeta():
      clock_(-1),
      importance_(0) { }

  RowOpLogMeta(const RowOpLogMeta &from):
  clock_(from.clock_),
  importance_(from.importance_) { }

  RowOpLogMeta & operator = (const RowOpLogMeta & from) {
    clock_ = from.clock_;
    importance_ = from.importance_;
    return *this;
  }

  int32_t get_clock() const {
    return clock_;
  }

  void set_clock(int32_t clock) {
    if (clock_ == -1 || clock < clock_)
      clock_ = clock;
  }

  void invalidate_clock() {
    clock_ = -1;
  }

  double get_importance() const {
    return importance_;
  }

  void set_importance(double importance) {
    importance_ = importance;
  }

  void accum_importance(double importance) {
    importance_ += importance;
  }

private:
  int32_t clock_;
  double importance_;
};

}
