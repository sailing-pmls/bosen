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
#include "petuum_ps/util/high_resolution_timer.hpp"
#include <limits>
#include <glog/logging.h>

namespace petuum {

HighResolutionTimer::HighResolutionTimer() {
  restart();
}

void HighResolutionTimer::restart() {
  int status = clock_gettime(CLOCK_MONOTONIC, &start_time_);
  CHECK_NE(-1, status) << "Couldn't initialize start_time_";
}

double HighResolutionTimer::elapsed() const {
  struct timespec now;
  int status = clock_gettime(CLOCK_MONOTONIC, &now);
  CHECK_NE(-1, status) << "Couldn't get current time.";

  double ret_sec = double(now.tv_sec - start_time_.tv_sec);
  double ret_nsec = double(now.tv_nsec - start_time_.tv_nsec);

  while (ret_nsec < 0) {
    ret_sec -= 1.0;
    ret_nsec += 1e9;
  }
  double ret = ret_sec + ret_nsec / 1e9;
  return ret;
}

double HighResolutionTimer::elapsed_max() const {
  return double((std::numeric_limits<double>::max)());
}

double HighResolutionTimer::elapsed_min() const {
  return 0.0;
}

}   // namespace petuum
