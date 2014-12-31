// Author: Dai Wei (wdai@cs.cmu.edu), Pengtao Xie (pxie@cs.cmu.edu)
// Date: 2014.10.04

#include "math_util.hpp"
#include <ml/util/fastonebigheader.h>
#include <glog/logging.h>
#include <cmath>

namespace util {

float SafeLog(float x) {
  if (std::abs(x) < 1e-15) {
    x = 1e-15;
  }
  return fastlog(x);
}

float LogSum(float log_a, float log_b) {
  return (log_a < log_b) ? log_b + fastlog(1 + fastexp(log_a - log_b)) :
    log_a + log(1 + fastexp(log_b-log_a));
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
		if (std::abs((*vec)[i]) < 1e-10) {
			(*vec)[i] = 1e-10;
    }
	}
	double lsum = LogSumVec(*vec);
	for (int i = 0; i < vec->size(); ++i) {
		(*vec)[i] = fastexp((*vec)[i] - lsum);
		//(*vec)[i] = exp((*vec)[i] - lsum);
    (*vec)[i] = (*vec)[i] > 1 ? 1. : (*vec)[i];
  }
}

float VectorDotProduct(const std::vector<float>& vec1,
    const std::vector<float>& vec2) {
  float sum = 0.;
  for (int i = 0; i < vec1.size(); ++i) {
    sum += vec1[i] * vec2[i];
  }
  return sum;
}

void VectorScaleAndAdd(float alpha, const std::vector<float>& vec1,
    std::vector<float>* vec2) {
  for (int i = 0; i < vec1.size(); ++i) {
   (*vec2)[i] += alpha * vec1[i];
  }
}

}  // namespace util
