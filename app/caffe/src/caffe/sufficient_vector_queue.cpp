#include <cstring>
#include "caffe/common.hpp"
#include "caffe/syncedmem.hpp"
#include "caffe/sufficient_vector.hpp"
#include "caffe/sufficient_vector_queue.hpp"
#include "caffe/util/math_functions.hpp"

namespace caffe {

SufficientVectorQueue::~SufficientVectorQueue() {
  while(!sv_queue_.empty()) {
    delete sv_queue_.front();
    sv_queue_.pop();
  }
}

void SufficientVectorQueue::Add(SufficientVector* v) {
  std::unique_lock<std::mutex> lock(mtx_);
  sv_queue_.push(v);
}

bool SufficientVectorQueue::Get(SufficientVector* v) {
  std::unique_lock<std::mutex> lock(mtx_);
  if (sv_queue_.empty()) {
    return false;
  }
  // copy the front to v
  SufficientVector* front = sv_queue_.front();
  CHECK_EQ(front->a_size(), v->a_size());
  CHECK_EQ(front->b_size(), v->b_size());
  memcpy(v->mutable_cpu_a(), front->cpu_a(), v->a_size()) ;
  memcpy(v->mutable_cpu_b(), front->cpu_b(), v->b_size()) ;
  
  if (++read_count_ >= max_read_count_) {
    delete front;
    sv_queue_.pop();
    read_count_ = 0;
  }
  return true;
}

template<typename Dtype>
bool SufficientVectorQueue::Get(SVProto* v) {
  std::unique_lock<std::mutex> lock(mtx_);
  if (sv_queue_.empty()) {
    return false;
  }
  // copy the front to v
  SufficientVector* front = sv_queue_.front();
  front->ToProto<Dtype>(v); 
 
  if (++read_count_ >= max_read_count_) {
    delete front;
    sv_queue_.pop();
    read_count_ = 0;
  }
  return true;
}

template bool SufficientVectorQueue::Get<float>(SVProto*);
template bool SufficientVectorQueue::Get<double>(SVProto*);

}  // namespace caffe
