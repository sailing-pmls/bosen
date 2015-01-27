// Author: Jiesi Zhao (jiesizhao0423@gmail.com), Wei Dai (wdai@cs.cmu.edu)
// Date: 2014.11.8

#pragma once

#include "common.hpp"
#include <vector>
#include <cstdint>
#include <ml/include/ml.hpp>
#include <memory>
#include "decision_tree.hpp"
#include "utils.hpp"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

namespace tree {

struct RandForestConfig {
  int32_t client_id;
  int32_t thread_id;
  int32_t num_threads;
  int32_t num_trees;
  bool save_trees;
  DecisionTreeConfig tree_config;
};

class RandForest {
public:
  RandForest(const RandForestConfig& config);

  // Train the trees
  void Train();

  // Save serialized trees into file
  void SaveTrees(std::string output_file);

  // Read trees from file
  void LoadTrees(std::string input_file);

  // Return the predicted x's label; optionally return the vote
  // (votes->size() will be num_labels_).
  int32_t Predict(const petuum::ml::AbstractFeature<float>& x,
      std::vector<int32_t>* votes = 0) const;

  void ComputeFeatureImportance(std::vector<float>& importance) const;

private:
  int32_t client_id_;

  int32_t thread_id_; 

  int32_t num_threads_;

  int32_t num_trees_;

  int32_t num_labels_;

  bool save_trees_;

  std::vector<DecisionTree * > trees_;

  DecisionTreeConfig tree_config_;

  std::vector<std::string> serial_trees_;

};


}  // namespace tree
