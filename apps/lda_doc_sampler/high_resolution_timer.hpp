#pragma once

#include "time.h"

namespace util {

// This is a simpler implementation of timer to replace
// boost::high_resolution_timer. Code based on
// http://boost.2283326.n4.nabble.com/boost-shared-mutex-performance-td2659061.html
class HighResolutionTimer {
  public:
    HighResolutionTimer();

    void restart();

    // return elapsed time in seconds
    double elapsed() const;

    // return estimated maximum value for elapsed()
    double elapsed_max() const;

    // return minimum value for elapsed()
    double elapsed_min() const;

  private:
    struct timespec start_time_;
};

} // namespace util
