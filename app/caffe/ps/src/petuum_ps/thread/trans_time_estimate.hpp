#pragma once

#include "petuum_ps/thread/context.hpp"
#include "petuum_ps_common/include/constants.hpp"

namespace petuum {

class TransTimeEstimate {
public:
  static double EstimateTransMillisec(size_t accum_sent_bytes,
                                      double bandwidth_mbps) {
    return (accum_sent_bytes * kNumBitsPerByte)
        / bandwidth_mbps / kOneThousand;
  }
};

}
