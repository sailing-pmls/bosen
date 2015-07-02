// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.10.04

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <ml/feature/sparse_feature.hpp>
#include <ml/feature/dense_feature.hpp>

namespace petuum {
namespace ml {

// Read dense binary data and labels from filename and output to 'features'
// (DenseFeature) and 'labels'. A label (int32_t) preceeds the data feature
// (float). features and labels will be resized to num_data.
void ReadDataLabelBinary(const std::string& filename,
    int32_t feature_dim, int32_t num_data,
    std::vector<AbstractFeature<float>*>* features, std::vector<int32_t>* labels,
    bool feature_one_based = false, bool label_one_based = false);

// Same as ReadDataLabelBinary, but read each feature as float vectors.
void ReadDataLabelBinary(const std::string& filename,
    int32_t feature_dim, int32_t num_data,
    std::vector<std::vector<float> >* features, std::vector<int32_t>* labels,
    bool feature_one_based = false, bool label_one_based = false);

// Similar to ReadDataLabelBinary, but read LibSVM format: label
// [feature_id:feature_value] as SparseFeature.
//
// Only read num_data if num_data < data in the file. If your feature id
// starts at 1 instead of 0, set feature_one_based = true.
void ReadDataLabelLibSVM(const std::string& filename,
    int32_t feature_dim, int32_t num_data,
    std::vector<AbstractFeature<float>*>* features, std::vector<int32_t>* labels,
    bool feature_one_based = false, bool label_one_based = false,
    bool snappy_compressed = false);

// Read real value labels (for regression).
void ReadDataLabelLibSVM(const std::string& filename,
    int32_t feature_dim, int32_t num_data,
    std::vector<AbstractFeature<float>*>* features, std::vector<float>* labels,
    bool feature_one_based = false, bool label_one_based = false,
    bool snappy_compressed = false);

// Read LibSVM into std::vector<float>. Categorical (int) labels.
void ReadDataLabelLibSVM(const std::string& filename,
    int32_t feature_dim, int32_t num_data,
    std::vector<std::vector<float> >* features, std::vector<int32_t>* labels,
    bool feature_one_based = false, bool label_one_based = false,
    bool snappy_compressed = false);

// Read LibSVM into std::vector<float>. Real value (float) labels.
void ReadDataLabelLibSVM(const std::string& filename,
    int32_t feature_dim, int32_t num_data,
    std::vector<std::vector<float> >* features, std::vector<float>* labels,
    bool feature_one_based = false, bool label_one_based = false,
    bool snappy_compressed = false);

}  // namespace ml
}  // namespace petuum
