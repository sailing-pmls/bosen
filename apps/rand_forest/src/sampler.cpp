#include "sampler.hpp"
#include <random>

namespace tree {

Sampler::Sampler() {
  std::random_device rd;
  rng_engine_.reset(new std::mt19937(rd()));
  zero_one_dist_ = std::uniform_real_distribution<double>(0, 1);
}

// John D. Cook, http://stackoverflow.com/a/311716/15485
std::vector<int> Sampler::SampleWithoutReplacement(int n, int k) {
  std::vector<int> samples(k);
  int t = 0; // total input records dealt with
  int m = 0; // number of items selected so far
  double u;

  while (m < k) {
    u = zero_one_dist_(*rng_engine_);

    if ( (n - t)*u >= k - m ) {
      t++;
    } else {
      samples[m] = t;
      t++;
      m++;
    }
  }
  return samples;
}

}  // namespace tree
