// Author: Jiesi Zhao (jiesizhao0423@gmail.com), Wei Dai (wdai@cs.cmu.edu)
// Date: 2014.11.5

#include "decision_tree.hpp"
#include "split_finder.hpp"
#include <glog/logging.h>
#include <vector>
#include <cstdint>
#include <ml/include/ml.hpp>
#include <algorithm>
#include <limits>

namespace tree {

void DecisionTree::Init(const DecisionTreeConfig& config) {
  CHECK(!root_) << "Tree is already built.";
  features_ = config.features;
  labels_ = config.labels;
  CHECK_NOTNULL(features_);
  CHECK_NOTNULL(labels_);
  num_data_ = features_->size();
  // Tree configs.
  max_depth_ = config.max_depth;
  num_data_subsample_ = config.num_data_subsample;
  num_features_subsample_ = config.num_features_subsample;
  num_labels_ = config.num_labels;
  feature_dim_ = config.feature_dim;

  CHECK_EQ(num_data_, labels_->size());
  std::random_device rd;
  rng_engine_.reset(new std::mt19937(rd()));

  std::vector<int32_t> data_idx(num_data_);
  for (int i = 0; i < num_data_; ++i) {
    data_idx[i] = i;
  }

  std::vector<int32_t> feature_ids(feature_dim_);
  for (int i = 0; i < feature_dim_; ++i) {
    feature_ids[i] = i;
  }
  root_.reset(RecursiveBuild(0, data_idx, feature_ids));
}

int32_t DecisionTree::Predict(
    const petuum::ml::AbstractFeature<float>& x) const {
  CHECK(root_) << "Cannot evaluate before Build() a tree.";
  return root_->Predict(x);
}

// ============== Private Methods =============

TreeNode* DecisionTree::RecursiveBuild(int32_t depth,
    const std::vector<int32_t>& available_data_idx,
    const std::vector<int32_t>& available_feature_ids, TreeNode* curr_node) {
  std::vector<int32_t> sub_data_idx = available_data_idx;

  if (curr_node == 0) {
    // Creating root.
    curr_node = new TreeNode();

    // Subsample data if we have more than num_data_subsample_ and num_data_subsample_ needs to be positive integer.
    if (num_data_subsample_ > 0 && available_data_idx.size() > num_data_subsample_) {
      std::vector<int32_t> available_data_idx_copy = available_data_idx;
      std::shuffle(available_data_idx_copy.begin(),
          available_data_idx_copy.end(), *rng_engine_);
      sub_data_idx = std::vector<int32_t>(available_data_idx_copy.begin(),
          available_data_idx_copy.begin() + num_data_subsample_);
    }
  }

  // Base case.
  if (depth == max_depth_ - 1 || available_feature_ids.size() == 1 ||
      AllSameLabels(available_data_idx)) {
    curr_node->SetLeafVal(ComputeLeafVal(available_data_idx));
    return curr_node;                                                                                                                                      
  }

  // Subsample features if we have more than num_features_subsample_ and num_features_subsample_ needs to be positive integer.
  std::vector<int32_t> sub_feature_ids;    
  std::vector<int32_t> available_feature_ids_copy = available_feature_ids;
  if (num_features_subsample_ > 0 && available_feature_ids.size() > num_features_subsample_) {    
    std::shuffle(available_feature_ids_copy.begin(),
        available_feature_ids_copy.end(), *rng_engine_);
    sub_feature_ids = std::vector<int32_t>(available_feature_ids_copy.begin(),
        available_feature_ids_copy.begin() + num_features_subsample_);
  }else{
    sub_feature_ids = available_feature_ids;
  }

  // Find a split.
  int32_t split_feature_id = 0;
  float split_feature_val = 0;
  int32_t split_feature_idx = FindSplit(sub_data_idx,
      sub_feature_ids, &split_feature_id, &split_feature_val);
  curr_node->Split(split_feature_id, split_feature_val);

  // Partition the data by split_feature_val.
  std::vector<int32_t> left_partition;
  std::vector<int32_t> right_partition;
  PartitionData(split_feature_id, split_feature_val, sub_data_idx,
      &left_partition, &right_partition);

  // Remove split_feature_id from available_feature_ids.
  available_feature_ids_copy[split_feature_idx] =
    available_feature_ids_copy[available_feature_ids.size() - 1];
  available_feature_ids_copy.pop_back();

  // Build left subtree (ignore the returned TreeNode*).
  TreeNode* left_child = curr_node->GetLeftChild();
  TreeNode* right_child = curr_node->GetRightChild();
  if (left_partition.size() == 0) {
    left_child->SetLeafVal(ComputeLeafVal(sub_data_idx));
  } else {
    RecursiveBuild(depth + 1, left_partition, available_feature_ids_copy,
      left_child);
  }
  if (right_partition.size() == 0) {
    right_child->SetLeafVal(ComputeLeafVal(sub_data_idx));
  } else {
    RecursiveBuild(depth + 1, right_partition, available_feature_ids_copy,
      right_child);
  }
  return curr_node;
}

int32_t DecisionTree::FindSplit(const std::vector<int32_t>& sub_data_idx,
    const std::vector<int32_t>& sub_feature_ids,
    int32_t* split_feature_id, float* split_feature_val) const {
  int32_t split_feature_idx = 0;
  float best_gain_ratio = std::numeric_limits<float>::min();

  for (int i = 0; i < sub_feature_ids.size(); ++i) {
    // For each feature, add feature val and label of each sample
    SplitFinder split_finder(num_labels_);
    for (int j = 0; j < sub_data_idx.size(); ++j) {
      split_finder.AddInstance(
          (*(*features_)[sub_data_idx[j]])[sub_feature_ids[i]],
          (*labels_)[sub_data_idx[j]]);
    }
    // Compute gain ratio of the feature
    float gain_ratio;
    float split_val = split_finder.FindSplitValue(&gain_ratio);
    // Compare gain ratio of different features
    if (gain_ratio > best_gain_ratio) {
      best_gain_ratio = gain_ratio;
      *split_feature_val = split_val;
      split_feature_idx = i;
    }

  }
  *split_feature_id = sub_feature_ids[split_feature_idx];
  return split_feature_idx;
}

void DecisionTree::PartitionData(int32_t feature_id, float feature_val,
    const std::vector<int32_t>& data_idx,
    std::vector<int32_t>* left_partition,
    std::vector<int32_t>* right_partition) const {
  left_partition->clear();
  right_partition->clear();
  for (int i = 0; i < data_idx.size(); ++i) {
    if ((*(*features_)[data_idx[i]])[feature_id] <= feature_val) {
      left_partition->push_back(data_idx[i]);
    } else {
      right_partition->push_back(data_idx[i]);
    }
  }
}

int32_t DecisionTree::ComputeLeafVal(const std::vector<int32_t>& data_idx)
  const {
    std::vector<int32_t> count_each_label(num_labels_);
    for (int i = 0; i < data_idx.size(); ++i) {
      int32_t label = (*labels_)[data_idx[i]];
      count_each_label[label]++;
    }
    int32_t max_label = 0;
    for (int i = 1; i < num_labels_; ++i) {
      if (count_each_label[i] > count_each_label[max_label]) {
        max_label = i;
      }
    }
    return max_label;
  }

bool DecisionTree::AllSameLabels(const std::vector<int32_t>& data_idx) const {
  if (data_idx.size() == 0) {
    return true;  // vacuously true.
  }
  int32_t label = (*labels_)[data_idx[0]];
  for (int i = 1; i < data_idx.size(); ++i) {
    if (label != (*labels_)[data_idx[i]]) {
      return false;
    }
  }
  return true;
}

}  // namespace tree
