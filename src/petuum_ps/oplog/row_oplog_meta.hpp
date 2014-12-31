#pragma once

#include <stdint.h>

namespace petuum {

class RowOpLogMeta {
public:
  RowOpLogMeta():
      clock_(-1),
      importance_(0) { }

  int32_t get_clock() const {
    return clock_;
  }

  void set_clock(int32_t clock) {
    if (clock_ == -1 || clock < clock_)
      clock_ = clock;
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
