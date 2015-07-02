// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.11.22

#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace petuum {
namespace ml {

template<typename DATUM>
class AbstractParser {
public:
  virtual ~AbstractParser() { }

  // Parse line and return # of bytes consumed.
  virtual DATUM* Parse(char const* line, int* num_bytes_used) = 0;
};

}  // namespace ml
}  // namespace petuum
