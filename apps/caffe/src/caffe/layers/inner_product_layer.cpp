#include <vector>

#include "caffe/blob.hpp"
#include "caffe/common.hpp"
#include "caffe/filler.hpp"
#include "caffe/layer.hpp"
#include "caffe/util/math_functions.hpp"
#include "caffe/vision_layers.hpp"
#include <petuum_ps_common/include/petuum_ps.hpp>

namespace caffe {

template <typename Dtype>
void InnerProductLayer<Dtype>::LayerSetUp(const vector<Blob<Dtype>*>& bottom,
    vector<Blob<Dtype>*>* top, const bool init_ps, int* num_tables,
    map<string, vector<int> >* layer_name_to_blob_global_idx) {
  const int num_output = this->layer_param_.inner_product_param().num_output();
  string name = this->layer_param_.name();
  bias_term_ = this->layer_param_.inner_product_param().bias_term();
  N_ = num_output;
  K_ = bottom[0]->count() / bottom[0]->num();
  // Check if we need to set up the weights
  if (this->blobs_.size() > 0) {
    LOG(INFO) << "Skipping parameter initialization";
  } else {
    if (bias_term_) {
      this->blobs_.resize(2);
      if (init_ps) {
        this->blob_global_id_.resize(2);
        (*layer_name_to_blob_global_idx)[name] = vector<int>(2);
      }  
    } else {
      this->blobs_.resize(1);
      if (init_ps) {
        this->blob_global_id_.resize(1);
        (*layer_name_to_blob_global_idx)[name] = vector<int>(1);
      }  
    }
    // Intialize the weight
    int weight_table_id = (init_ps ? *num_tables : -1);
    this->blobs_[0].reset
	  (new Blob<Dtype>(1, 1, N_, K_, BlobProto_BlobMode_GLOBAL, weight_table_id));
    if (init_ps) {
      // Create the weight table
      this->blobs_[0]->CreatePSTable();
      this->blob_global_id_[0] = weight_table_id;
      (*layer_name_to_blob_global_idx)[name][0] = weight_table_id;
      ++(*num_tables);
    }
    // If necessary, intiialize the bias term
    if (bias_term_) {
      int bias_table_id = (init_ps ? *num_tables : -1);
      this->blobs_[1].reset
	    (new Blob<Dtype>(1, 1, 1, N_, BlobProto_BlobMode_GLOBAL, bias_table_id));
      if (init_ps) {
        // Create the bias table
        this->blobs_[1]->CreatePSTable();
        this->blob_global_id_[1] = bias_table_id;
        (*layer_name_to_blob_global_idx)[name][1] = bias_table_id;
        ++(*num_tables);
      }
    }
  }  // parameter initialization
  this->param_propagate_down_.resize(this->blobs_.size(), true);
}


template <typename Dtype>
void InnerProductLayer<Dtype>::Reshape(const vector<Blob<Dtype>*>& bottom,
      vector<Blob<Dtype>*>* top) {
  // Figure out the dimensions
  M_ = bottom[0]->num();
  CHECK_EQ(bottom[0]->count() / bottom[0]->num(), K_) << "Input size "
    "incompatible with inner product parameters.";
  (*top)[0]->Reshape(bottom[0]->num(), N_, 1, 1);
  // Set up the bias multiplier
  if (bias_term_) {
    bias_multiplier_.Reshape(1, 1, 1, M_);
    caffe_set(M_, Dtype(1), bias_multiplier_.mutable_cpu_data());
  }
}

template <typename Dtype>
void InnerProductLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
    vector<Blob<Dtype>*>* top) {
  const Dtype* bottom_data = bottom[0]->cpu_data();
  Dtype* top_data = (*top)[0]->mutable_cpu_data();
  const Dtype* weight = this->blobs_[0]->cpu_data();
  caffe_cpu_gemm<Dtype>(CblasNoTrans, CblasTrans, M_, N_, K_, (Dtype)1.,
      bottom_data, weight, (Dtype)0., top_data);
  if (bias_term_) {
    caffe_cpu_gemm<Dtype>(CblasNoTrans, CblasNoTrans, M_, N_, 1, (Dtype)1.,
        bias_multiplier_.cpu_data(),
        this->blobs_[1]->cpu_data(), (Dtype)1., top_data);
  }
}

template <typename Dtype>
void InnerProductLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
    const vector<bool>& propagate_down,
    vector<Blob<Dtype>*>* bottom) {
  if (this->param_propagate_down_[0]) {
    const Dtype* top_diff = top[0]->cpu_diff();
    const Dtype* bottom_data = (*bottom)[0]->cpu_data();
    // Gradient with respect to weight
    caffe_cpu_gemm<Dtype>(CblasTrans, CblasNoTrans, N_, K_, M_, (Dtype)1.,
        top_diff, bottom_data, (Dtype)0., this->blobs_[0]->mutable_cpu_diff());
  }
  if (bias_term_ && this->param_propagate_down_[1]) {
    const Dtype* top_diff = top[0]->cpu_diff();
    // Gradient with respect to bias
    caffe_cpu_gemv<Dtype>(CblasTrans, M_, N_, (Dtype)1., top_diff,
        bias_multiplier_.cpu_data(), (Dtype)0.,
        this->blobs_[1]->mutable_cpu_diff());
  }
  if (propagate_down[0]) {
    const Dtype* top_diff = top[0]->cpu_diff();
    // Gradient with respect to bottom data
    caffe_cpu_gemm<Dtype>(CblasNoTrans, CblasNoTrans, M_, K_, N_, (Dtype)1.,
        top_diff, this->blobs_[0]->cpu_data(), (Dtype)0.,
        (*bottom)[0]->mutable_cpu_diff());
  }
}

#ifdef CPU_ONLY
STUB_GPU(InnerProductLayer);
#endif

INSTANTIATE_CLASS(InnerProductLayer);

}  // namespace caffe
