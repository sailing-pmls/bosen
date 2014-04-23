// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.04.06

#pragma once

#include <vector>
#include <cstdint>

namespace lda {

class LDADocument {
public:
  LDADocument();

  int32_t get_topic(int32_t 

private:
  std::vector<int32_t> tokens_;
  std::vector<int32_t> topics_;

}


}  // namespace lda
