// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.10.21

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <sstream>
#include <glog/logging.h>
#include <ml/feature/abstract_feature.hpp>

namespace petuum {
namespace ml {

// DenseFeature stores only feature values on std::vector.
template<typename V>
class DenseFeature : public AbstractFeature<V> {
public:
  DenseFeature(const std::vector<V>& val) :
    AbstractFeature<V>(val.size()), data_(val) { }

  DenseFeature(int32_t feature_dim, const V& default_val = V(0)) :
    AbstractFeature<V>(feature_dim), data_(feature_dim, default_val) { }

  DenseFeature(const std::vector<int32_t>& feature_ids,
      const std::vector<V>& val, int32_t feature_dim) :
    AbstractFeature<V>(feature_dim), data_(feature_dim) {
      for (int i = 0; i < feature_ids.size(); ++i) {
        data_[feature_ids[i]] = val[i];
      }
  }

  void Init(const std::vector<V>& val) {
    this->feature_dim_ = val.size();
    data_ = val;
  }

  // Makes it behave like std::vector.
  inline V& operator[](int32_t feature_id) {
    return data_[feature_id];
  }

  inline const std::vector<V>& GetVector() const {
    return data_;
  }

  inline std::vector<V>& GetVector() {
    return data_;
  }

public:  // AbstractFeature override.
  DenseFeature() { }

  DenseFeature(const DenseFeature<V>& other) :
    AbstractFeature<V>(other.data_.size()), data_(other.data_) { }

  DenseFeature<V>& operator=(const DenseFeature<V>& other) {
    data_ = other.data_;
    this->feature_dim_ = data_.size();
    return *this;
  }

  inline V operator[](int32_t feature_id) const {
    return data_[feature_id];
  }

  void SetFeatureVal(int32_t feature_id, const V& val) {
    data_[feature_id] = val;
  }

  // Number of (non-zero) entries in the underlying vector.
  inline int32_t GetNumEntries() const {
    return data_.size();
  }

  // idx is the same as feature id in DenseFeature.
  inline int32_t GetFeatureId(int32_t idx) const {
    return idx;
  }

  inline V GetFeatureVal(int32_t idx) const {
    return data_[idx];
  }

  std::string ToString() const {
    std::stringstream ss;
    for (int i = 0; i < data_.size(); ++i) {
      ss << i << ":" << data_[i] << " ";
    }
    ss << "(feature dim: " << this->feature_dim_ << ")";
    return ss.str();
  }

protected:
  std::vector<V> data_;
};

}  // namespace ml
}  // namespace petuum
