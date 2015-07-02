// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.10.21

#include <gtest/gtest.h>
#include <ml/feature/sparse_feature.hpp>

namespace petuum {
namespace ml {

TEST(SparseFeatureTest, SmokeTests) {
  SparseFeature<float> sp1;
  int num_elem = 5;
  std::vector<int> idx(num_elem);
  std::vector<float> vals(num_elem);
  for (int i = 0; i < num_elem; ++i) {
    idx[i] = i*2;
    vals[i] = i*i;
  }
  int feature_dim = 20;
  SparseFeature<float> sp2(idx, vals, feature_dim);
  sp1 = sp2;    // copy constructor.
  EXPECT_EQ(feature_dim, sp1.GetFeatureDim());
  EXPECT_EQ(feature_dim, sp2.GetFeatureDim());
  EXPECT_EQ(num_elem, sp1.GetNumEntries());
  EXPECT_EQ(num_elem, sp2.GetNumEntries());
  for (int i = 0; i < sp1.GetNumEntries(); ++i) {
    EXPECT_EQ(sp1.GetFeatureId(i), sp2.GetFeatureId(i));
    EXPECT_EQ(sp1.GetFeatureVal(i), sp2.GetFeatureVal(i));
    int feature_id = sp1.GetFeatureId(i);
    EXPECT_EQ(sp1[feature_id], sp2[feature_id]);
  }
  LOG(INFO) << "sp1: " << sp1.ToString();

  EXPECT_EQ(0, sp1[1]);  // Odd number feature id should be 0.

  sp1.SetFeatureVal(2, 0);  // zero out a non-zero entry.
  EXPECT_EQ(num_elem - 1, sp1.GetNumEntries());
  EXPECT_EQ(4, sp1[4]);  // other entry should be unaffected.
}

TEST(SparseFeatureTest, SimpleConstructorTests) {
  int feature_dim = 10;
  {
    SparseFeature<float> sp1(feature_dim);
    EXPECT_EQ(0, sp1[9]);
    sp1.SetFeatureVal(9, 1.);
    EXPECT_EQ(1., sp1[9]);
  }
  {
    SparseFeature<float> sp1;
    sp1.Init(feature_dim);
    EXPECT_EQ(0, sp1[9]);
    sp1.SetFeatureVal(9, 1.);
    EXPECT_EQ(1., sp1[9]);
  }

}

}  // namespace ml
}  // namespace petuum
