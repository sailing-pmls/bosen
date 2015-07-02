// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.10.18

#pragma once

#include <memory>
#include <cstdint>
#include <ml/feature/abstract_feature.hpp>

namespace petuum {
namespace ml {

template<typename V, typename LABEL = int32_t>
class AbstractDatum {
public:
  AbstractDatum(AbstractFeature<V>* feature, LABEL label) :
    feature_(feature), label_(label) { }

  // Copy constructor transfer ownership of feature_.
  AbstractDatum(const AbstractDatum& other) :
    feature_(other.ReleaseFeature()), label_(other.label_) { }

  AbstractFeature<V>* GetFeature() {
    CHECK(feature_) << "Accessing null feature.";
    return feature_.get();
  }

  LABEL GetLabel() {
    return label_;
  }

  // Release ownership of feature.
  AbstractFeature<V>* ReleaseFeature() {
    return feature_.release();
  }

private:
  // Takes ownership of feature_.
  std::unique_ptr<AbstractFeature<V> > feature_;

  LABEL label_;
};

}  // namespace ml
}  // namespace petuum
