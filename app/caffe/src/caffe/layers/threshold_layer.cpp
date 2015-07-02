#include <vector>

#include "caffe/layer.hpp"
#include "caffe/vision_layers.hpp"


namespace caffe {

template <typename Dtype>
void ThresholdLayer<Dtype>::LayerSetUp(const vector<Blob<Dtype>*>& bottom,
    vector<Blob<Dtype>*>* top, const bool init_ps, int* num_tables,
    map<string, vector<int> >* layer_name_to_blob_global_idx) {
  NeuronLayer<Dtype>::LayerSetUp(bottom, top, init_ps, num_tables,
      layer_name_to_blob_global_idx);
  threshold_ = this->layer_param_.threshold_param().threshold();
}

template <typename Dtype>
void ThresholdLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
    vector<Blob<Dtype>*>* top) {
  const Dtype* bottom_data = bottom[0]->cpu_data();
  Dtype* top_data = (*top)[0]->mutable_cpu_data();
  const int count = bottom[0]->count();
  for (int i = 0; i < count; ++i) {
    top_data[i] = (bottom_data[i] > threshold_) ? Dtype(1) : Dtype(0);
  }
}

#ifdef CPU_ONLY
STUB_GPU_FORWARD(ThresholdLayer, Forward);
#endif

INSTANTIATE_CLASS(ThresholdLayer);

}  // namespace caffe
