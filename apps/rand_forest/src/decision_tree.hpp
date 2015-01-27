// Author: Jiesi Zhao (jiesizhao0423@gmail.com), Wei Dai (wdai@cs.cmu.edu)
// Date: 2014.11.5

#pragma once

#include <vector>
#include <cstdint>
#include <ml/include/ml.hpp>
#include "tree_node.hpp"
#include "utils.hpp"
#include <random>
#include <memory>
#include "split_finder.hpp"
#include <string>
#include <sstream>

namespace tree {

struct DecisionTreeConfig {
  // Maximum depth (the max # of nodes in a path including the root node).
  int32_t max_depth;

  // Sample this many data in constructing each split. 0 to use all.
  int32_t num_data_subsample;

  // Sample this many features (among remaining features) to consider when
  // doing split. 0 to use all.
  int32_t num_features_subsample;

  // # of classes / labels in the data set.
  int32_t num_labels;

  // # of features in the data set.
  int32_t feature_dim;

  // Data
  std::vector<petuum::ml::AbstractFeature<float>*>* features;
  std::vector<int32_t>* labels;
};

class DecisionTree {
public:
  DecisionTree() : features_(0), labels_(0) { };
  
  // Construct a tree from string of serialized tree
  DecisionTree(std::string input);

  // Construct the decision tree.
  void Init(const DecisionTreeConfig& config);

  // Predict x's label. Will fail if Build() isn't called yet.
  int32_t Predict(const petuum::ml::AbstractFeature<float>& x) const;

  // Compute feature importance after building the tree.
  void ComputeFeatureImportance(std::vector<float>& importance) const;
  // Get serialized tree as a string
  std::string GetSerializedTree();

  // Deserialize the tree from string
  void Deserialize(TreeNode *p, std::istringstream &in);

private:    // private methods.
  // Internal build method.
  TreeNode* RecursiveBuild(int32_t depth,
      const std::vector<int32_t>& available_data_idx,
      const std::vector<int32_t>& available_feature_ids,
      TreeNode* curr_node = 0);

  // Find the feature (among sub_feature_ids) to split and split value using
  // subset of data (sub_data_idx). Return idx such that sub_feature_ids[idx]
  // = *split_feature_id.
  int32_t FindSplit(const std::vector<int32_t>& sub_data_idx,
      const std::vector<int32_t>& sub_feature_ids,
      int32_t* split_feature_id, float* split_feature_val, float* gain_ratio) const;

  // Partition 'data_idx' into left_partition (whose 'feature_id' feature
  // <= feature_val), and right_partition.
  void PartitionData(int32_t feature_id, float feature_val,
      const std::vector<int32_t>& data_idx,
      std::vector<int32_t>* left_partition,
      std::vector<int32_t>* right_partition) const;

  // Find majority vote to get leaf value.
  int32_t ComputeLeafVal(const std::vector<int32_t>& data_idx) const;

  // True if all labels in data_idx are the same.
  bool AllSameLabels(const std::vector<int32_t>& data_idx) const;

  // Serialize the tree with pre-order traversal
  void Serialize(TreeNode *p, std::string &out);

private:
  // Underlying data (features and labels).
  const std::vector<petuum::ml::AbstractFeature<float>*>* features_;
  const std::vector<int32_t>* labels_;
  int32_t num_data_;

  std::unique_ptr<TreeNode> root_;

  // Random number generator engine.
  std::unique_ptr<std::mt19937> rng_engine_;

  // Tree parameters (see DecisionTreeConfig)
  int32_t max_depth_;
  int32_t num_data_subsample_;
  int32_t num_features_subsample_;
  int32_t num_labels_;
  int32_t feature_dim_;

};

}  // namespace tree
