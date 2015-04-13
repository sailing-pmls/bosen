// Author: Jiesi Zhao (jiesizhao0423@gmail.com), Wei Dai (wdai@cs.cmu.edu)
// Date: 2014.11.5

#pragma once

#include <vector>
#include <cstdint>

namespace tree {

// a <feature_val, label, weight> triplet.
struct FeatureEntry {
  float feature_val;
  int32_t label;
  float weight;   // can represent counts as well.
};

// SplitFinder finds the split of a feature using gain ratio criterion.
class SplitFinder {
public:
  SplitFinder(int32_t num_labels);

  // Add an instance with feature_val (continuous) and label. It will just
  // append the instance to the entries_.
  void AddInstance(float feature_val, int32_t label, float weight = 1.);

  // Add an instance with feature_val (continuous) and label. The entries_
  // will merge same instances.
  void AddInstanceDedup(float feature_val, int32_t label, float weight = 1.);

  // Select a split value to maximize computing gain ratio.
  // Return best split value for a continuous feature. Optionally return
  // gain_ratio.
  float FindSplitValue(float* gain_ratio = 0);

private:  // private functions
  friend class SplitFinderTest;
  // Find minimun and maximum of feature value
  void FindMinMaxFeature(float *min_value, float *max_value);

  // Return the gain ratio for this split_val.
  float ComputeGainRatio(float split_val);

private:
  std::vector<FeatureEntry> entries_;

  // # of output labels.
  int32_t num_labels_;

  // Entropy of label (i.e., entropy before split).
  float pre_split_entropy_;
};

}  // namespace tree
