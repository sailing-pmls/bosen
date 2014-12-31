// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2013.11.15

#pragma once

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/thread.hpp>
#include <list>

namespace petuum {

struct WeightItem {
  float weight;
  int32_t item_id;
  WeightItem(float w, int32_t id) : weight(w), item_id(id) { }
};

// Thread-safe.
class DeltaSampler {
  public:
    DeltaSampler();

    // Add an item, assuming item_id does not already exist. If so, there will
    // be duplicate...
    void AddItem(int32_t item_id, float weight);

    // Sample one item using weight and remove that item.
    int32_t SampleOneAndRemove();

    // Get number of weight items.
    int32_t num_items() const;

  private:
    // uniform_zero_one_ generates random number on [0, 1)..
    boost::mt19937 rng_;    // Random number engine
    boost::uniform_real<float> dist_;   // Uniform distribution on [0, 1).
    boost::variate_generator<boost::mt19937&, boost::uniform_real<float> >
      uniform_zero_one_;

    // Sum of all weights in weights_.
    float total_weight_;

    // A sorted (tree) map from weight to item_id from large weight to small
    // weight.
    std::list<WeightItem> weights_;

    // Protects weights_ and total_weight_
    mutable boost::mutex weight_mutex_;
};

}  // namespace petuum
