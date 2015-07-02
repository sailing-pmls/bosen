// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.10.18

#pragma once

#include <memory>
#include <type_traits>

namespace petuum {
namespace ml {

// Usage pattern:
//  SparseFeature<float> sf;
//  //...populate sf.
//  for (int i = 0; i < sf.GetNumEntries(); ++i) {
//    int feature_id = sf.GetFeatureId(i);
//    float feature_val = sf.GetFeatureVal(i);
//    // ... use (feature_id, feature_val) pair.
//    sf.SetFeatureVal(i, new_val);
//  }
//
// For DenseFeature, it's also efficient to use [] operator:
//  DenseFeature<float> df;
//  for (int i = 0; i < sf.GetFeatureDim(); ++i) {
//    float feature_val = df[i];
//  }
template<typename V>
class AbstractFeature {
public:
  AbstractFeature() : feature_dim_(0) { }
  virtual ~AbstractFeature() { }

  AbstractFeature(int32_t feature_dim) : feature_dim_(feature_dim) { }

  // Feature dimension.
  int32_t GetFeatureDim() const { return feature_dim_; }

  // Get feature value from feature id.
  virtual V operator[](int32_t feature_id) const = 0;

  // Setter for feature value using feature id.
  virtual void SetFeatureVal(int32_t feature_id, const V& val) = 0;

  // Number of non-zero entries in the feature. For dense feature it assume
  // all entries are non-zero.
  virtual int32_t GetNumEntries() const = 0;

  // =========== GetFeatureId and GetFeatureVal simulate an iterator =========
  // Get the idx-th feature id (idx is different from feature_id in
  // operator[]).
  virtual int32_t GetFeatureId(int32_t idx) const = 0;

  // Get the idx-th feature value.
  virtual V GetFeatureVal(int32_t idx) const = 0;

  // print out the feature in readable format.
  virtual std::string ToString() const = 0;

  static_assert(std::is_pod<V>::value, "V must be POD");

protected:
  int32_t feature_dim_;   // feature dimension.
};

}  // namespace ml
}  // namespace petuum
