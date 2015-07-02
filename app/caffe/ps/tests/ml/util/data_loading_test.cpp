// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.11.5

#include <gtest/gtest.h>
#include <ml/util/data_loading.hpp>
#include <ml/feature/abstract_feature.hpp>
#include <ml/feature/sparse_feature.hpp>
#include <string>
#include <cstdint>

namespace petuum {
namespace ml {

const std::string kLibSVMFile = "tests/ml/util/sample_data.libsvm";
const int32_t kFeatureDim = 4;
const bool kFeatureOneBased = true;
const bool kLabelOneBased = true;
const int32_t kNumData = 4;

TEST(DataLoadingTest, ReadLibSVMToSparseFeature) {
  std::vector<petuum::ml::AbstractFeature<float>*> features;
  std::vector<int32_t> labels;
  petuum::ml::ReadDataLabelLibSVM(kLibSVMFile, kFeatureDim, kNumData,
      &features, &labels, kFeatureOneBased, kLabelOneBased);
  // Check labels.
  EXPECT_EQ(0, labels[0]);
  EXPECT_EQ(2, labels[1]);
  EXPECT_EQ(1, labels[2]);
  EXPECT_EQ(0, labels[3]);

  // Check data 0.
  int data_id = 0;
  EXPECT_EQ(3.5, (*features[data_id])[0]);
  EXPECT_EQ(0, (*features[data_id])[1]);
  EXPECT_NEAR(-9.8, (*features[data_id])[3], 1e-4);

  // Check data 2
  data_id = 2;
  EXPECT_EQ(0, (*features[data_id])[0]);
  EXPECT_EQ(0, (*features[data_id])[1]);

  // Check data 3
  data_id = 3;
  EXPECT_EQ(76, (*features[data_id])[2]);
}

TEST(DataLoadingTest, ReadLibSVMToVector) {
  std::vector<std::vector<float> > features;
  std::vector<int32_t> labels;
  petuum::ml::ReadDataLabelLibSVM(kLibSVMFile, kFeatureDim, kNumData,
      &features, &labels, kFeatureOneBased, kLabelOneBased);
  // Check labels.
  EXPECT_EQ(0, labels[0]);
  EXPECT_EQ(2, labels[1]);
  EXPECT_EQ(1, labels[2]);
  EXPECT_EQ(0, labels[3]);

  // Check data 0.
  int data_id = 0;
  EXPECT_EQ(3.5, features[data_id][0]);
  EXPECT_EQ(0, features[data_id][1]);
  EXPECT_NEAR(-9.8, features[data_id][3], 1e-4);

  // Check data 2
  data_id = 2;
  EXPECT_EQ(0, features[data_id][0]);
  EXPECT_EQ(0, features[data_id][1]);

  // Check data 3
  data_id = 3;
  EXPECT_EQ(76, features[data_id][2]);
}

}  // namespace ml
}  // namespace petuum
