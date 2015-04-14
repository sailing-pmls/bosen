// Author: Jiesi Zhao (jiesizhao0423@gmail.com), Wei Dai (wdai@cs.cmu.edu)
// Date: 2014.11.5

#include "common.hpp"
#include "split_finder.hpp"
#include "utils.hpp"
#include <algorithm>
#include <glog/logging.h>
#include <limits>
#include <random>
#include <math.h>

namespace tree {

SplitFinder::SplitFinder(int32_t num_labels) :
  num_labels_(num_labels), pre_split_entropy_(0.) { }

void SplitFinder::AddInstance(float feature_val, int32_t label,
    float weight) {
  FeatureEntry new_entry;
  new_entry.feature_val = feature_val;
  new_entry.label = label;
  new_entry.weight = weight;
  entries_.push_back(new_entry);
}

void SplitFinder::AddInstanceDedup(float feature_val,
    int32_t label, float weight) {
  // To be optimized
  for (auto iter = entries_.begin(); iter != entries_.end(); ++iter) {
    if ((iter->feature_val == feature_val)
        && (iter->label == label)) {
      iter->weight += weight;
      return;
    }
  }

  // This is a new feature_val-label combo.
  AddInstance(feature_val, label, weight);
}

float SplitFinder::FindSplitValue(float* gain_ratio) {
  // Compute entropy of label
  std::vector<float> label_distribution(num_labels_, 0);
  for (auto iter = entries_.begin(); iter != entries_.end(); ++iter) {
    label_distribution[iter->label] += 1;
  }
  Normalize(&label_distribution);
  pre_split_entropy_ = ComputeEntropy(label_distribution);

  float min_value = std::numeric_limits<float>::max();
  float max_value = std::numeric_limits<float>::min();
  FindMinMaxFeature(&min_value, &max_value);

  std::random_device rd;
  std::mt19937 mt(rd());
  std::uniform_real_distribution<float> dist(min_value, max_value);

  float best_gain_ratio = std::numeric_limits<float>::min();
  float best_split_val = min_value;
  for (int i = 0; i < FLAGS_num_feat_split_vals; ++i) {
      // Randomly generate a split threshold
      float rand_split = dist(mt);
      float gain_ratio = ComputeGainRatio(rand_split);

      if (gain_ratio > best_gain_ratio) {
        best_gain_ratio = gain_ratio;
        best_split_val = rand_split;
      }
    }
  if (gain_ratio != 0) {
    *gain_ratio = best_gain_ratio;
  }
  return best_split_val;
}

// ================== Private Functions ===============

void SplitFinder::FindMinMaxFeature(float *min_value, float *max_value) {
  for (int i = 0; i < entries_.size(); ++i) {
    if (*min_value > entries_[i].feature_val) {
      *min_value = entries_[i].feature_val;
    }
    if (*max_value < entries_[i].feature_val) {
      *max_value = entries_[i].feature_val;
    }
  }
}

float SplitFinder::ComputeGainRatio(float split_val) {
  std::vector<float> left_dist(num_labels_);    // left distribution.
  std::vector<float> right_dist(num_labels_);
  float left_dist_weight = 0.;
  float right_dist_weight = 0.;
  for (int i = 0; i < entries_.size(); ++i) {
    if (entries_[i].feature_val <= split_val) {
      left_dist[entries_[i].label] += entries_[i].weight;
      left_dist_weight += entries_[i].weight;
    } else {
      right_dist[entries_[i].label] += entries_[i].weight;
      right_dist_weight += entries_[i].weight;
    }
  }

  // Normalize
  Normalize(&left_dist);
  Normalize(&right_dist);

  // Compute entropy
  float left_entropy = ComputeEntropy(left_dist);
  float right_entropy = ComputeEntropy(right_dist);

  // Compute conditional entropy
  std::vector<float> split_dist;
  split_dist.push_back(left_dist_weight /
      (left_dist_weight + right_dist_weight));
  split_dist.push_back(right_dist_weight /
      (left_dist_weight + right_dist_weight));
  float cond_entropy = split_dist[0] * left_entropy +
    split_dist[1] * right_entropy;

  // information gain
  float info_gain = pre_split_entropy_ - cond_entropy;
  Normalize(&split_dist);
  float splitinfo = ComputeEntropy(split_dist);
  float gain_ratio = info_gain / splitinfo;
  if (splitinfo == 0.0) {
    gain_ratio = 0.0;
  }
  return gain_ratio;
}

}  // namespace tree
