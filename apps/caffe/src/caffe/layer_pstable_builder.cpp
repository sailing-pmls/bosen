#include "caffe/layer.hpp"
#include "caffe/proto/caffe.pb.h"
#include <petuum_ps_common/include/petuum_ps.hpp>

namespace caffe {

const int GetNumGlobalTables(const LayerParameter& param) {
  const LayerParameter_LayerType& type = param.type();
  int num_global_tables = 0;
  if (type == LayerParameter_LayerType_INNER_PRODUCT) {
    const bool bias_term = param.inner_product_param().bias_term();
    num_global_tables = (bias_term ? 2 : 1);
  } else if (type == LayerParameter_LayerType_CONVOLUTION) {
    const bool bias_term = param.convolution_param().bias_term();
    num_global_tables = (bias_term ? 2 : 1);
  } 
  return num_global_tables;
}

}
