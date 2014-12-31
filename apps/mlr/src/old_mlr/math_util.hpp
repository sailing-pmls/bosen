// Author: Dai Wei (wdai@cs.cmu.edu), Pengtao Xie (pxie@cs.cmu.edu)
// Date: 2014.10.04

#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace util {

// Apply cutoff for x -> 0.
float SafeLog(float x);

// Compute log(a + b) from log(a) and log(b) using the identity
// log(a + b) = log(a) + log(1 + (b/a))
float LogSum(float log_a, float log_b);

// Return log{\sum_i exp(logvec[i])}.
float LogSumVec(const std::vector<float>& logvec);

// vec[i] = softmax(vec[i], vec) = exp(vec[i]) / \sum_k exp(vec[k]).
void Softmax(std::vector<float>* vec);

// vec1.size() must equal vec2.size(). No check is performed.
float VectorDotProduct(const std::vector<float>& vec1,
    const std::vector<float>& vec2);

// vec2 += alpha * vec1 (similar to BLAS)
// vec1.size() must equal vec2->size(). No check is performed.
void VectorScaleAndAdd(float alpha, const std::vector<float>& vec1,
    std::vector<float>* vec2);

}  // namespace util
