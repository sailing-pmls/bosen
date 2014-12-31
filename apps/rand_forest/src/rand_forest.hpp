// Author: Jiesi Zhao (jiesizhao0423@gmail.com), Wei Dai (wdai@cs.cmu.edu)
// Date: 2014.11.8

#pragma once

#include <vector>
#include <cstdint>
#include <ml/include/ml.hpp>
#include <memory>
#include "decision_tree.hpp"

namespace tree {

struct RandForestConfig {
  int32_t num_trees;
  DecisionTreeConfig tree_config;
};

class RandForest {
public:
  RandForest(const RandForestConfig& config);

  // Return the predicted x's label; optionally return the vote
  // (votes->size() will be num_labels_).
  int32_t Predict(const petuum::ml::AbstractFeature<float>& x,
      std::vector<int32_t>* votes = 0) const;

private:
  int32_t num_trees_;

  int32_t num_labels_;

  std::vector<DecisionTree> trees_;

  DecisionTreeConfig tree_config_;
};


}  // namespace tree
