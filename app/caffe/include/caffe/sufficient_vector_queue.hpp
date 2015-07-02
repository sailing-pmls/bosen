#ifndef CAFFE_SUFFICIENT_VECTOR_QUEUE_HPP_
#define CAFFE_SUFFICIENT_VECTOR_QUEUE_HPP_

#include <queue>
#include <mutex>
#include "caffe/proto/caffe.pb.h"
#include "caffe/common.hpp"
#include "caffe/sufficient_vector.hpp"

namespace caffe {

using std::queue;

/**
 * @brief 
 */
class SufficientVectorQueue {
 public:
  SufficientVectorQueue(const int max_read_count)
      : read_count_(0), max_read_count_(max_read_count) { }
  ~SufficientVectorQueue();

  void Add(SufficientVector* v);
  bool Get(SufficientVector* v);
  template<typename Dtype>
  bool Get(SVProto* v);
  inline int size() { return sv_queue_.size(); }

 private:
  queue<SufficientVector*> sv_queue_;
  int read_count_; // the number of times the front sv is read
  const int max_read_count_;
  std::mutex mtx_;
};  // class SufficientVectorQueue 


}  // namespace caffe

#endif  // CAFFE_SUFFICIENT_VECTOR_QUEUE_HPP_
