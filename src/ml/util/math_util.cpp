// Author: Dai Wei (wdai@cs.cmu.edu), Pengtao Xie (pxie@cs.cmu.edu)
// Date: 2014.10.21

#include <ml/util/math_util.hpp>
#include <ml/util/fastapprox/fastapprox.hpp>
#include <glog/logging.h>
#include <cmath>
#include <sstream>

namespace petuum {
namespace ml {

namespace {

const float kCutoff = 1e-15;

}  // anonymous namespace

float SafeLog(float x) {
  if (std::abs(x) < kCutoff) {
    x = kCutoff;
  }
  return fastlog(x);
}

float LogSum(float log_a, float log_b) {
  return (log_a < log_b) ? log_b + fastlog(1 + fastexp(log_a - log_b)) :
    log_a + fastlog(1 + fastexp(log_b-log_a));
}

float LogSumVec(const std::vector<float>& logvec) {
	float sum = 0.;
	sum = logvec[0];
	for (int i = 1; i < logvec.size(); ++i) {
		sum = LogSum(sum, logvec[i]);
	}
	return sum;
}

void Softmax(std::vector<float>* vec) {
  CHECK_NOTNULL(vec);
  // TODO(wdai): Figure out why this is necessary. Doubt it is.
	for (int i = 0; i < vec->size(); ++i) {
		if (std::abs((*vec)[i]) < kCutoff) {
			(*vec)[i] = kCutoff;
    }
	}
	double lsum = LogSumVec(*vec);
	for (int i = 0; i < vec->size(); ++i) {
		(*vec)[i] = fastexp((*vec)[i] - lsum);
		//(*vec)[i] = exp((*vec)[i] - lsum);
    (*vec)[i] = (*vec)[i] > 1 ? 1. : (*vec)[i];
  }
}

float DenseDenseFeatureDotProduct(const AbstractFeature<float>& f1,
    const AbstractFeature<float>& f2) {
  CHECK_EQ(f1.GetFeatureDim(), f2.GetFeatureDim());
  auto f1_dense_ptr = static_cast<const DenseFeature<float>*>(&f1);
  auto f2_dense_ptr = static_cast<const DenseFeature<float>*>(&f2);
  const std::vector<float>& v1 = f1_dense_ptr->GetVector();
  const std::vector<float>& v2 = f2_dense_ptr->GetVector();
  float sum = 0.;
  for (int i = 0; i < v1.size(); ++i) {
    sum += v1[i] * v2[i];
  }
  return sum;
}

float DenseSparseFeatureDotProduct(const AbstractFeature<float>& f1,
    const AbstractFeature<float>& f2) {
  return SparseAnyFeatureDotProduct(f2, f1);
}

float SparseAnyFeatureDotProduct(const AbstractFeature<float>& f1,
    const AbstractFeature<float>& f2) {
  CHECK_EQ(f1.GetFeatureDim(), f2.GetFeatureDim());
  int j = 0;
  float sum = 0.;
  int f2_num_entries = f2.GetNumEntries();
  for (int i = 0; i < f1.GetNumEntries() && j < f2_num_entries; ++i) {
    int32_t f1_fid = f1.GetFeatureId(i);
    while (f2.GetFeatureId(j) < f1_fid && j < f2_num_entries) {
      ++j;
    }
    if (f1_fid == f2.GetFeatureId(j)) {
      sum += f1.GetFeatureVal(i) * f2.GetFeatureVal(j);
    }
  }
  return sum;
}

void FeatureScaleAndAdd(float alpha, const DenseFeature<float>& f1,
    DenseFeature<float>* f2) {
  const std::vector<float>& f1_vec = f1.GetVector();
  std::vector<float>& f2_vec = f2->GetVector();
  for (int i = 0; i < f1_vec.size(); ++i) {
   f2_vec[i] += alpha * f1_vec[i];
  }
}

void FeatureScaleAndAdd(float alpha, const AbstractFeature<float>& f1,
    AbstractFeature<float>* f2) {
  CHECK_EQ(f1.GetFeatureDim(), f2->GetFeatureDim());
  for (int i = 0; i < f1.GetNumEntries(); ++i) {
    int32_t f1_fid = f1.GetFeatureId(i);
    f2->SetFeatureVal(f1_fid, alpha * f1.GetFeatureVal(i) + (*f2)[f1_fid]);
  }
}


}  // namespace ml
}  // namespace petuum
