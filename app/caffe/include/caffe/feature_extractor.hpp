#ifndef FEATURE_EXTRACTOR_HPP_
#define FEATURE_EXTRACTOR_HPP_

#include <string>
#include <vector>
#include <fstream>
#include "caffe/net.hpp"
#include "caffe/proto/caffe.pb.h"
#include "caffe/util/io.hpp"
#include "caffe/caffe.hpp"

namespace caffe {

template <typename Dtype>
class FeatureExtractor {
 public:
  explicit FeatureExtractor(
      const map<string, vector<int> >* layer_blobs_global_idx_ptr,
      const int thread_id) : 
      layer_blobs_global_idx_ptr_(layer_blobs_global_idx_ptr), 
      thread_id_(thread_id) { }

  void ExtractFeatures(const NetParameter& net_param);

 protected:
  const map<string, vector<int> >* layer_blobs_global_idx_ptr_; 
  int thread_id_;

  DISABLE_COPY_AND_ASSIGN(FeatureExtractor);
};

}  // namespace caffe

#endif  // FEATURE_EXTRACTOR_HPP_
