// Copyright (c) 2013, SailingLab
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

#include "utils.hpp"

#include "math.h"
#include <time.h>

namespace {
const double cof[6] = { 76.18009172947146, -86.50532032941677,
                       24.01409824083091, -1.231739572450155,
                       0.1208650973866179e-2, -0.5395239384953e-5
                     };
}

namespace lda {

double LogGamma(double xx) {
  int j;
  double x, y, tmp1, ser;
  y = xx;
  x = xx;
  tmp1 = x+5.5;
  tmp1 -= (x+0.5)*log(tmp1);
  ser = 1.000000000190015;
  for (j = 0; j < 6; j++) ser += cof[j]/++y;
  return -tmp1+log(2.5066282746310005*ser/x);
}

double get_time() {
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC, &start);
  return (start.tv_sec + start.tv_nsec/1000000000.0);
}

}
