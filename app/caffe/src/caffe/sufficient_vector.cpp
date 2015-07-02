#include <cstring>

#include "caffe/sufficient_vector.hpp"

namespace caffe {

SufficientVector::SufficientVector(
    const size_t a_size, const size_t b_size, const int layer_id) {
  Reshape(a_size, b_size);
  layer_id_ = layer_id;
}

SufficientVector::~SufficientVector() {
  a_.reset();
  b_.reset();
}

void SufficientVector::Reshape(
    const size_t a_size, const size_t b_size) {
  CHECK_GE(a_size, 0);
  CHECK_GE(b_size, 0);
  a_size_ = a_size;
  b_size_ = b_size;
  a_.reset(new SyncedMemory(a_size_));
  b_.reset(new SyncedMemory(b_size_));
}

const void* SufficientVector::cpu_a() const {
  CHECK(a_);
  return a_->cpu_data();
}
const void* SufficientVector::gpu_a() const {
  CHECK(a_);
  return a_->gpu_data();
}
const void* SufficientVector::cpu_b() const {
  CHECK(b_);
  return b_->cpu_data();
}
const void* SufficientVector::gpu_b() const {
  CHECK(b_);
  return b_->gpu_data();
}

void* SufficientVector::mutable_cpu_a() {
  CHECK(a_);
  return a_->mutable_cpu_data();
}
void* SufficientVector::mutable_gpu_a() {
  CHECK(a_);
  return a_->mutable_gpu_data();
}
void* SufficientVector::mutable_cpu_b() {
  CHECK(b_);
  return b_->mutable_cpu_data();
}
void* SufficientVector::mutable_gpu_b() {
  CHECK(b_);
  return b_->mutable_gpu_data();
}

template <typename Dtype>
void SufficientVector::FromProto(const SVProto& proto) {
  Reshape(proto.a_size() * sizeof(Dtype), proto.b_size() * sizeof(Dtype));
  layer_id_ = proto.layer_id();

  Dtype* a_vec = static_cast<Dtype*>(mutable_cpu_a());
  for (int i = 0; i < proto.a_size(); ++i) {
    a_vec[i] = proto.a(i);
  }
  Dtype* b_vec = static_cast<Dtype*>(mutable_cpu_b());
  for (int i = 0; i < proto.b_size(); ++i) {
    b_vec[i] = proto.b(i);
  }
}

template void SufficientVector::FromProto<float>(const SVProto&);
template void SufficientVector::FromProto<double>(const SVProto&);


template<typename Dtype>
void SufficientVector::ToProto(SVProto* proto) const {
  proto->set_layer_id(layer_id_);
  proto->clear_a();
  proto->clear_b();
  const Dtype* a_vec = static_cast<const Dtype*>(cpu_a());
  for (int i = 0; i < a_size_ / sizeof(Dtype); ++i) {
    proto->add_a(a_vec[i]);
  }
  const Dtype* b_vec = static_cast<const Dtype*>(cpu_b());
  for (int i = 0; i < b_size_ / sizeof(Dtype); ++i) {
    proto->add_b(b_vec[i]);
  }
}

template void SufficientVector::ToProto<float>(SVProto*) const;
template void SufficientVector::ToProto<double>(SVProto*) const;

}  // namespace caffe
