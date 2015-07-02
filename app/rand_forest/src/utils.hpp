// Author: Jiesi Zhao (jiesizhao0423@gmail.com), Wei Dai (wdai@cs.cmu.edu)
// Date: 2014.11.4


#pragma once
#include <vector>

namespace tree {

// Compute Entropy of distribution (assuming \sum_i dist[i] = 1).
float ComputeEntropy(const std::vector<float> &dist);

// Normalize
void Normalize(std::vector<float>* count);

}  // namespace tree
