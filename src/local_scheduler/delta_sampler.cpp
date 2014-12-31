// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2013.11.15

#include "local_scheduler/delta_sampler.hpp"
#include <glog/logging.h>
#include <time.h>

namespace petuum {

namespace {

// Comparator to keep the weight list sorted.
struct WeightItemGreaterThan {
  bool operator()(const WeightItem& a, const WeightItem& b) const {
    return a.weight > b.weight;
  }
};

WeightItemGreaterThan weight_item_comp;

}   // anonymous namespace

// ================ Public Methods ===============

DeltaSampler::DeltaSampler() :
  rng_(time(NULL)), dist_(0, 1), uniform_zero_one_(rng_, dist_),
  total_weight_(0) {
}

void DeltaSampler::AddItem(int32_t item_id, float weight) {
  boost::lock_guard<boost::mutex> lock(weight_mutex_);
  weights_.push_back(WeightItem(weight, item_id));
  //weights_.sort(weight_item_comp);
  total_weight_ += weight;
}

int32_t DeltaSampler::num_items() const {
  boost::lock_guard<boost::mutex> lock(weight_mutex_);
  int32_t size = weights_.size();
  return size;
}

int32_t DeltaSampler::SampleOneAndRemove() {
  boost::lock_guard<boost::mutex> lock(weight_mutex_);

  float rand = uniform_zero_one_() * total_weight_;
  std::list<WeightItem>::iterator iter = weights_.begin();
  float sum = iter->weight;
  for (; sum < rand; ++iter, sum += iter->weight);

  int32_t sampled_item_id = iter->item_id;
  total_weight_ -= iter->weight;
  weights_.erase(iter);
  return sampled_item_id;
}

}  // namespace petuum
