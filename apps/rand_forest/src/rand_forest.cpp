// Author: Jiesi Zhao (jiesizhao0423@gmail.com), Wei Dai (wdai@cs.cmu.edu)
// Date: 2014.11.8

#include "rand_forest.hpp"

namespace tree {

RandForest::RandForest(const RandForestConfig& config) :
  num_trees_(config.num_trees), num_labels_(config.tree_config.num_labels),
  trees_(num_trees_), tree_config_(config.tree_config) {
    for (int i = 0; i < num_trees_; ++i) {
      // Build each tree.
      trees_[i].Init(tree_config_);
      LOG(INFO) << "Built tree NO. " << i;
    }
  }


int32_t RandForest::Predict(const petuum::ml::AbstractFeature<float>& x,
    std::vector<int32_t>* votes) const {
  std::vector<int32_t> votes_copy(num_labels_);
  for (int i = 0; i < num_trees_; ++i) {
    int32_t pred_label = trees_[i].Predict(x);
    ++votes_copy[pred_label];
  }

  int32_t max_label = 0;
  int32_t max_vote = votes_copy[0];
  for (int j = 1; j < num_labels_; ++j) {
    if (votes_copy[j] > max_vote) {
      max_label = j;
      max_vote = votes_copy[j];
    }
  }
  if (votes != 0) {
    *votes = votes_copy;
  }
  return max_label;
}

}  // namespace tree
