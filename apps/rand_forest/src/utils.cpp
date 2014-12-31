// Author: Jiesi Zhao (jiesizhao0423@gmail.com), Wei Dai (wdai@cs.cmu.edu)
// Date: 2014.11.5

#include "utils.hpp"
#include <math.h>
#include <vector>
#include <glog/logging.h>
#include <ml/include/ml.hpp>

namespace tree {

// Compute Entropy of distribution (assuming \sum_i dist[i] = 1).
float ComputeEntropy(const std::vector<float> &dist) {
  float entropy = 0.0;
  for (auto iter = dist.begin(); iter != dist.end(); ++iter) {
    if (*iter) {
      entropy -= (*iter) * fastlog2(*iter);
    }
  }
  if (entropy < 1e-5) {
    entropy = 0.0;
  }
  return entropy;
}

// Normalize
void Normalize(std::vector<float>* count) {
  float sum = 0.;
  for (auto iter = count->begin(); iter != count->end(); ++iter) {
    sum += (*iter);
  }
  for (auto iter = count->begin(); iter != count->end(); ++iter) {
    (*iter) /= sum;
  }
}

}  // namespace tree
