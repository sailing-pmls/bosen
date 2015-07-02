// Author: Dai Wei (wdai@cs.cmu.edu), Pengtao Xie (pxie@cs.cmu.edu)
// Date: 2014.10.21

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <ml/feature/abstract_feature.hpp>
#include <ml/feature/dense_feature.hpp>
#include <ml/feature/sparse_feature.hpp>
#include <sstream>

namespace petuum {
namespace ml {

// Convert vector v to space separated string without '\n'.
template<typename V>
std::string VectorToString(const std::vector<V>& v) {
  std::stringstream ss;
  for (int i = 0; i < v.size(); ++i) {
    ss << v[i] << " ";
  }
  return ss.str();
}

// Apply cutoff for x -> 0.
float SafeLog(float x);

float Sigmoid(float x);

// Compute log(a + b) from log(a) and log(b) using the identity
// log(a + b) = log(a) + log(1 + (b/a))
float LogSum(float log_a, float log_b);

// Return log{\sum_i exp(logvec[i])}.
float LogSumVec(const std::vector<float>& logvec);

// vec[i] = softmax(vec[i], vec) = exp(vec[i]) / \sum_k exp(vec[k]).
void Softmax(std::vector<float>* vec);

// Specialize DenseFeature dot products to make it 11~12x faster than the
// sparse version.
float DenseDenseFeatureDotProduct(const AbstractFeature<float>& f1,
    const AbstractFeature<float>& f2);

// Use this version if f1 is dense and f2 is sparse. It's 15% faster than
// reverting the two.
float DenseSparseFeatureDotProduct(const AbstractFeature<float>& f1,
    const AbstractFeature<float>& f2);

float SparseDenseFeatureDotProduct(const AbstractFeature<float>& f1,
    const AbstractFeature<float>& f2);

// If f1 is dense and f2 is sparse, it is 3x slower than the other way around.
//
// Comment (wdai): If we swap the two based on GetNumEntries(), it is about 15%
// slower than sparse-dense version.
float SparseSparseFeatureDotProduct(const AbstractFeature<float>& f1,
    const AbstractFeature<float>& f2);

// DenseFeature specialization.
void FeatureScaleAndAdd(float alpha, const DenseFeature<float>& f1,
    DenseFeature<float>* f2);

// f2 += alpha * f1 (similar to BLAS).
void FeatureScaleAndAdd(float alpha, const AbstractFeature<float>& f1,
    AbstractFeature<float>* f2);

}  // namespace ml
}  // namespace petuum
