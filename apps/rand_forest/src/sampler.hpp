#pragma once

#include <random>
#include <memory>
#include <vector>

namespace tree {

class Sampler {
public:
  Sampler();

  // Sample k unique items from population n.
  std::vector<int> SampleWithoutReplacement(int n, int k);

private:
  std::unique_ptr<std::mt19937> rng_engine_;
  std::uniform_real_distribution<double> zero_one_dist_;
};

}   // namespace tree
