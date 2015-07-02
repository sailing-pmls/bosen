// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.10.21

#include <gtest/gtest.h>
#include <ml/feature/dense_feature.hpp>

namespace petuum {
namespace ml {

TEST(DenseFeatureTest, SmokeTests) {
  DenseFeature<float> feature1;
  int num_elem = 5;
  std::vector<float> vals(num_elem);
  for (int i = 0; i < num_elem; ++i) {
    vals[i] = i*i;
  }
  DenseFeature<float> feature2(vals);
  feature1 = feature2;    // copy constructor.
  for (int i = 0; i < num_elem; ++i) {
    EXPECT_EQ(i*i, feature1[i]);
  }
  EXPECT_EQ(num_elem, feature1.GetFeatureDim());
  EXPECT_EQ(num_elem, feature2.GetFeatureDim());
  EXPECT_EQ(num_elem, feature1.GetNumEntries());
  EXPECT_EQ(num_elem, feature2.GetNumEntries());
  for (int i = 0; i < feature1.GetNumEntries(); ++i) {
    EXPECT_EQ(feature1.GetFeatureId(i), feature2.GetFeatureId(i));
    EXPECT_EQ(feature1.GetFeatureVal(i), feature2.GetFeatureVal(i));
    int feature_id = feature1.GetFeatureId(i);
    EXPECT_EQ(feature1[feature_id], feature2[feature_id]);
  }
  LOG(INFO) << "feature1: " << feature1.ToString();

  feature1.SetFeatureVal(2, -1);
  EXPECT_EQ(-1, feature1[2]);

  feature1[3] = -2;
  EXPECT_EQ(-2, feature1[3]);
}

}  // namespace ml
}  // namespace petuum
