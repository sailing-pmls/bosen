// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2015.03.12

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <cmath>
#include <glog/logging.h>
#include <ml/feature/dense_feature.hpp>

namespace petuum {
namespace ml {

namespace {

// Initial # of entries for decay_rates_.
const int kNumClocksDefault = 20000;

}   // anonymous namespace

// DenseDecayFeature stores only feature values on std::vector and support
// efficient sparse weight decay:
//  w <-- decay_rate * w
// Common usage:
// DenseDecayFeature<float> ddf(...);
//   ddf.Decay();
//   ddf[0] += gradient;
template<typename V>
class DenseDecayFeature : public DenseFeature<V> {
public:
  DenseDecayFeature(const std::vector<V>& val) :
    DenseFeature<V>(val), clock_(0), param_clocks_(val.size()),
    current_(true), decay_rates_(kNumClocksDefault, 1.), decay_rate_(1.) { }

  DenseDecayFeature(int32_t feature_dim, const V& default_val = V(0)) :
    DenseFeature<V>(feature_dim, default_val), clock_(0), param_clocks_(feature_dim),
    current_(true), decay_rates_(kNumClocksDefault, 1.), decay_rate_(1.) { }

  DenseDecayFeature(const std::vector<int32_t>& feature_ids,
      const std::vector<V>& val, int32_t feature_dim) :
    DenseFeature<V>(feature_ids, val, feature_dim), clock_(0), param_clocks_(feature_dim),
    current_(true), decay_rates_(kNumClocksDefault, 1.), decay_rate_(1.) {
    }

  // Makes it behave like std::vector.
  inline V& operator[](int32_t feature_id) {
    ApplyDecay(feature_id);
    return this->data_[feature_id];
  }

  inline const std::vector<V>& GetVector() const {
    if (!current_) {
      ApplyDecayAll();
    }
    return this->data_;
  }

  inline std::vector<V>& GetVector() {
    if (!current_) {
      ApplyDecayAll();
    }
    return this->data_;
  }

  // This will apply all existing decay rates and start over.
  void SetDecayRate(double decay_rate, int num_clocks = kNumClocksDefault) {
    ApplyDecayAll();
    decay_rate_ = decay_rate;
    decay_rates_.resize(num_clocks + 1);
    decay_rates_[0] = 1.;
    for (int c = 1; c <= num_clocks; ++c) {
      decay_rates_[c] = decay_rates_[c - 1] * decay_rate_;
    }
  }

  void Decay() {
    ++clock_;
    if (clock_ >= decay_rates_.size()) {
      // Expand decay_rates_.
      SetDecayRate(decay_rate_, std::ceil(decay_rates_.size() * 1.5));
    }
    // Once we clock there are some entry that are not decayed.
    current_ = false;
  }

public:  // DenseFeature override.
  DenseDecayFeature() : clock_(0), current_(true) {
    SetDecayRate(1.);
  }

  DenseDecayFeature(const DenseDecayFeature<V>& other) {
    *this = other;
  }

  DenseDecayFeature<V>& operator=(const DenseDecayFeature<V>& other) {
    decay_rate_ = other.decay_rate_;
    decay_rates_ = other.decay_rates_;
    clock_ = other.clock_;
    param_clocks_ = other.param_clocks_;
    current_ = other.current_;
    this->data_ = other.data_;
    this->feature_dim_ = this->data_.size();
    ApplyDecayAll();
    return *this;
  }

  inline V operator[](int32_t feature_id) const {
    ApplyDecay(feature_id);
    return this->data_[feature_id];
  }

  void SetFeatureVal(int32_t feature_id, const V& val) {
    this->data_[feature_id] = val;
    param_clocks_[feature_id] = clock_;   // make it current.
  }

  inline V GetFeatureVal(int32_t idx) const {
    ApplyDecay(idx);
    return this->data_[idx];
  }

  std::string ToString() const {
    ApplyDecayAll();
    return DenseFeature<V>::ToString();
  }
private:
  // Apply decay to a feature.
  void ApplyDecay(int feature_id) const {
    this->data_[feature_id] *=
      decay_rates_[clock_ - param_clocks_[feature_id]];
    param_clocks_[feature_id] = clock_;
  }

  // Apply all pending decay and zero the clock.
  inline void ApplyDecayAll() const {
    if (current_) return;
    for (int i = 0; i < this->feature_dim_; ++i) {
      this->data_[i] *= decay_rates_[clock_ - param_clocks_[i]];
    }
    // this->data_ is current now.
    std::fill(param_clocks_.begin(), param_clocks_.end(), 0);
    clock_ = 0;
    current_ = true;
  }

private:
  // Each time Decay() is called, the row_clock_ increments by
  // 1. (row_clock_ - param_clocks_[i]) is the number of decays pending for
  // data_[i].
  mutable int32_t clock_;
  mutable std::vector<V> param_clocks_;

  // True if all param_clocks_ are current with clock_.
  mutable bool current_;

  // Cache all decay rates. decay_rates_[c] is the factor for c
  // clock differences between clock_ and param_clocks_[i].
  mutable std::vector<double> decay_rates_;
  double decay_rate_;
};

}  // namespace ml
}  // namespace petuum
