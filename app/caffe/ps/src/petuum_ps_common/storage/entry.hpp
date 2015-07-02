// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.10.16

#pragma once

#include <type_traits>
#include <cstdint>

namespace petuum {

// An Entry is an ID (int32_t) value (V) pair.
//
// Comment(wdai): We cannot use std::pair since we use memcpy and memmove and
// std::pair isn't POD.
template<typename V>
struct Entry {
  static_assert(std::is_pod<V>::value, "V must be POD");
  int32_t first;
  V second;
};

}  // namespace petuum
