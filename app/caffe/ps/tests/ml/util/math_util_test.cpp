// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.10.21

#include <gtest/gtest.h>
#include <algorithm>
#include <ml/feature/sparse_feature.hpp>
#include <ml/feature/dense_feature.hpp>
#include <ml/util/math_util.hpp>
#include <petuum_ps_common/util/high_resolution_timer.hpp>

namespace petuum {
namespace ml {

TEST(MathUtilTest, FeatureDotProduct) {
  // Sparse feature product
  SparseFeature<float> f1;
  int num_elem = 5;
  std::vector<int> feature_ids(num_elem);
  std::vector<float> vals(num_elem);
  float f1_dot_f1 = 0.;
  for (int i = 0; i < num_elem; ++i) {
    feature_ids[i] = i*2;
    vals[i] = i*i;
    f1_dot_f1 += vals[i] * vals[i];
  }
  int feature_dim = 20;
  f1 = SparseFeature<float>(feature_ids, vals, feature_dim);

  int num_elem2 = 2;
  std::vector<int> idx2(num_elem2);
  std::vector<float> vals2(num_elem2);
  idx2[0] = 2;
  vals2[0] = 3;
  idx2[1] = 3;
  vals2[1] = 4;
  SparseFeature<float> f2(idx2, vals2, feature_dim);
  float f1_dot_f2 = 0.;
  for (int i = 0; i < feature_dim; ++i) {
    f1_dot_f2 += f1[i] * f2[i];
  }

  EXPECT_EQ(f1_dot_f1, SparseAnyFeatureDotProduct(f1, f1));
  EXPECT_EQ(f1_dot_f2, SparseAnyFeatureDotProduct(f1, f2));
  EXPECT_EQ(f1_dot_f2, SparseAnyFeatureDotProduct(f2, f1));

  // Dense feature product
  std::vector<float> vals3(feature_dim);
  float f3_dot_f3 = 0.;
  for (int i = 0; i < feature_dim; ++i) {
    vals3[i] = i*i;
    f3_dot_f3 += vals3[i] * vals3[i];
  }
  DenseFeature<float> f3(vals3);
  EXPECT_EQ(f3_dot_f3, SparseAnyFeatureDotProduct(f3, f3));

  float f2_dot_f3 = 0.;
  for (int i = 0; i < feature_dim; ++i) {
    f2_dot_f3 += f2[i] * f3[i];
  }
  EXPECT_EQ(f2_dot_f3, SparseAnyFeatureDotProduct(f2, f3));
  EXPECT_EQ(f2_dot_f3, SparseAnyFeatureDotProduct(f3, f2));
}

namespace {

float VectorDotProduct(const std::vector<float>& vec1,
    const std::vector<float>& vec2) {
  float sum = 0.;
  for (int i = 0; i < vec1.size(); ++i) {
    sum += vec1[i] * vec2[i];
  }
  return sum;
}

float ArrayDotProduct(const float* arr1,
    const float* arr2, int dim) {
  float sum = 0.;
  for (int i = 0; i < dim; ++i) {
    sum += arr1[i] * arr2[i];
  }
  return sum;
}

}  // anonymous namespace

TEST(MathUtilTest, FeatureDotProductPerformance) {
  int num_data = 10000;
  int feature_dim = 10000;
  int num_trials = 3;
  std::vector<std::vector<float> > vec_features(num_data);
  for (int i = 0; i < num_data; ++i) {
    vec_features[i].resize(feature_dim);
    for (int j = 0; j < feature_dim; ++j) {
      vec_features[i][j] = i;
    }
  }

  // Build plain old array.
  float** arr_features = new float*[num_data];
  for (int i = 0; i < num_data; ++i) {
    arr_features[i] = vec_features[i].data();
  }

  std::vector<DenseFeature<float> > dense_features(num_data);
  for (int i = 0; i < num_data; ++i) {
    dense_features[i] = DenseFeature<float>(vec_features[i]);
  }

  for (int t = 0; t < num_trials; ++t) {
    std::vector<float> dot_vals1(num_data - 1);
    petuum::HighResolutionTimer timer;
    for (int i = 0; i < num_data - 1; ++i) {
      dot_vals1[i] = VectorDotProduct(vec_features[i], vec_features[i + 1]);
    }
    LOG(INFO) << "std::vector dot product completes in " << timer.elapsed();

    std::vector<float> dot_vals_arr(num_data - 1);
    timer.restart();
    for (int i = 0; i < num_data - 1; ++i) {
      dot_vals_arr[i] = ArrayDotProduct(arr_features[i], arr_features[i + 1],
          feature_dim);
    }
    LOG(INFO) << "array dot product completes in " << timer.elapsed();

    std::vector<float> dot_vals2(num_data - 1);
    timer.restart();
    for (int i = 0; i < num_data - 1; ++i) {
      dot_vals2[i] = DenseDenseFeatureDotProduct(dense_features[i],
          dense_features[i + 1]);
    }
    LOG(INFO) << "DenseFeature dot product completes in " << timer.elapsed();
    for (int i = 0; i < num_data - 1; ++i) {
      EXPECT_EQ(dot_vals1[i], dot_vals2[i]);
      EXPECT_EQ(dot_vals_arr[i], dot_vals2[i]);
    }
  }

  // Dense-sparse product.
  LOG(INFO) << " ------ Sparse-dense dot product ------";
  int num_elem = 100;
  std::vector<SparseFeature<float> > sparse_features(num_data);
  for (int i = 0; i < num_data; ++i) {
    std::vector<int> feature_ids(num_elem);
    std::vector<float> vals(num_elem);
    for (int j = 0; j < num_elem; ++j) {
      feature_ids[j] = j*90;
      vals[j] = j*j + i;
    }
    sparse_features[i] = SparseFeature<float>(feature_ids, vals, feature_dim);
  }

  for (int t = 0; t < num_trials; ++t) {
    std::vector<float> dot_vals1(num_data);
    petuum::HighResolutionTimer timer;
    for (int i = 0; i < num_data; ++i) {
      dot_vals1[i] = DenseSparseFeatureDotProduct(dense_features[i],
          sparse_features[i]);
    }
    LOG(INFO) << "dense-sparse dot product completes in " << timer.elapsed();

    std::vector<float> dot_vals2(num_data);
    timer.restart();
    for (int i = 0; i < num_data; ++i) {
      dot_vals2[i] = SparseAnyFeatureDotProduct(sparse_features[i],
          dense_features[i]);
    }
    LOG(INFO) << "sparse-dense dot product completes in " << timer.elapsed();
    for (int i = 0; i < num_data; ++i) {
      EXPECT_EQ(dot_vals1[i], dot_vals2[i]);
    }
  }

  // sparse-sparse product
  LOG(INFO) << " ------ Sparse-sparse dot product (1% sparsity) ------";
  for (int t = 0; t < num_trials; ++t) {
    std::vector<float> dot_vals1(num_data);
    petuum::HighResolutionTimer timer;
    for (int i = 0; i < num_data; ++i) {
      dot_vals1[i] = SparseAnyFeatureDotProduct(sparse_features[i],
          sparse_features[i]);
    }
    LOG(INFO) << "sparse-sparse dot product completes in " << timer.elapsed();
  }
}

TEST(MathUtilTest, FeatureScaleAndAdd) {
  int num_elem = 3;
  int feature_dim = 10;
  std::vector<int> feature_ids(num_elem);
  std::vector<float> vals(num_elem);
  for (int j = 0; j < num_elem; ++j) {
    feature_ids[j] = j * 2;
  }
  std::fill(vals.begin(), vals.end(), 2.);
  SparseFeature<float> f1(feature_ids, vals, feature_dim);

  // f2 is all 1's.
  std::vector<float> vals2(feature_dim);
  std::fill(vals2.begin(), vals2.end(), 1.);
  DenseFeature<float> f2(vals2);

  FeatureScaleAndAdd(10, f1, &f2);

  for (int j = 0; j < feature_dim; ++j) {
    if (j == 0 || j == 2 || j == 4) {
      EXPECT_EQ(21, f2[j]);
    } else {
      EXPECT_EQ(1., f2[j]);
    }
  }

  DenseFeature<float> f3(vals2);
  FeatureScaleAndAdd(10, f3, &f1);
  for (int j = 0; j < feature_dim; ++j) {
    if (j == 0 || j == 2 || j == 4) {
      EXPECT_EQ(12, f1[j]) << "j: " << j;
    } else {
      EXPECT_EQ(10., f1[j]) << "j: " << j;
    }
  }
}

}  // namespace ml
}  // namespace petuum
