#pragma once

#include <stdint.h>

namespace petuum {

const size_t kNumBitsPerByte = 8;
const size_t k1_Mi = 1024*1024;
const size_t k1_Ki = 1024;
const float kCuckooExpansionFactor = 1.428;

const size_t kOneThousand = 1000;

const uint64_t kMaxPendingMsgs = 200;
const uint64_t kMaxPendingAcks = 40;
}
