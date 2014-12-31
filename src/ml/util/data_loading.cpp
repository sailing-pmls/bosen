// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.10.04

#include <glog/logging.h>
#include <fstream>
#include <vector>
#include <cstdint>
#include <string>
#include <sstream>
#include <petuum_ps_common/util/high_resolution_timer.hpp>
#include <ml/util/data_loading.hpp>
#include <snappy.h>
#include <iterator>

namespace petuum {
namespace ml {

namespace {

const int32_t base = 10;

}  // anonymous namespace

void ReadDataLabelBinary(const std::string& filename,
    int32_t feature_dim, int32_t num_data,
    std::vector<AbstractFeature<float>*>* features,
    std::vector<int32_t>* labels,
    bool feature_one_based, bool label_one_based) {
  petuum::HighResolutionTimer read_timer;
  features->resize(num_data);
  labels->resize(num_data);
  std::ifstream is(filename, std::ifstream::binary);
  CHECK(is) << "Failed to open " << filename;
  std::vector<float> cache(feature_dim);
  for (int i = 0; i < num_data; ++i) {
    // Read the label
    is.read(reinterpret_cast<char*>(&(*labels)[i]), sizeof(int32_t));
    if (label_one_based) {
      --(*labels)[i];
    }
    // Read the feature
    is.read(reinterpret_cast<char*>(cache.data()), sizeof(float) * feature_dim);
    if (feature_one_based) {
      for (int j = 0; j < feature_dim; ++j) {
        --cache[j];
      }
    }
    (*features)[i] = new DenseFeature<float>(cache);
  }
  LOG(INFO) << "Read " << num_data << " instances from " << filename << " in "
    << read_timer.elapsed() << " seconds.";
}

void ReadDataLabelBinary(const std::string& filename,
    int32_t feature_dim, int32_t num_data,
    std::vector<std::vector<float> >* features, std::vector<int32_t>* labels,
    bool feature_one_based, bool label_one_based) {
  petuum::HighResolutionTimer read_timer;
  features->resize(num_data);
  labels->resize(num_data);
  std::ifstream is(filename, std::ifstream::binary);
  CHECK(is) << "Failed to open " << filename;
  for (int i = 0; i < num_data; ++i) {
    // Read the label
    is.read(reinterpret_cast<char*>(&(*labels)[i]), sizeof(int32_t));
    if (label_one_based) {
      --(*labels)[i];
    }
    // Read the feature
    (*features)[i].resize(feature_dim);
    is.read(reinterpret_cast<char*>((*features)[i].data()),
        sizeof(float) * feature_dim);
    if (feature_one_based) {
      for (int j = 0; j < feature_dim; ++j) {
        --(*features)[i][j];
      }
    }
  }
  LOG(INFO) << "Read " << num_data << " instances from " << filename << " in "
    << read_timer.elapsed() << " seconds.";
}

namespace {

// Return the label. feature_one_based = true assumes
// feature index starts from 1. Analogous for label_one_based.
int32_t ParseLibSVMLine(const std::string& line, std::vector<int32_t>* feature_ids,
    std::vector<float>* feature_vals, bool feature_one_based,
    bool label_one_based) {
  feature_ids->clear();
  feature_vals->clear();
  char *ptr = 0, *endptr = 0;
  // Read label.
  int label = strtol(line.data(), &endptr, base);
  label = label_one_based ? label - 1 : label;
  ptr = endptr;

  while (isspace(*ptr) && ptr - line.data() < line.size()) ++ptr;
  while (ptr - line.data() < line.size()) {
    // read a feature_id:feature_val pair
    int32_t feature_id = strtol(ptr, &endptr, base);
    if (feature_one_based) {
      --feature_id;
    }
    feature_ids->push_back(feature_id);
    ptr = endptr;
    CHECK_EQ(':', *ptr);
    ++ptr;

    feature_vals->push_back(strtod(ptr, &endptr));
    ptr = endptr;
    while (isspace(*ptr) && ptr - line.data() < line.size()) ++ptr;
  }
  return label;
}

// Open 'filename' and snappy decompress it to std::string.
std::string SnappyOpenFileToString(const std::string& filename) {
  std::ifstream file(filename, std::ifstream::binary);
  CHECK(file) << "Can't open " << filename;
  std::string buffer((std::istreambuf_iterator<char>(file)),
      std::istreambuf_iterator<char>());
  std::string uncompressed;
  CHECK(snappy::Uncompress(buffer.data(), buffer.size(), &uncompressed))
    << "Cannot snappy decompress buffer of size " << buffer.size()
    << "; File: " << filename;
  return uncompressed;
}

std::string OpenFileToString(const std::string& filename) {
  std::ifstream file(filename);
  CHECK(file) << "Can't open " << filename;
  std::string buffer((std::istreambuf_iterator<char>(file)),
      std::istreambuf_iterator<char>());
  return buffer;
}

}  // anonymous namespace

void ReadDataLabelLibSVM(const std::string& filename,
    int32_t feature_dim, int32_t num_data,
    std::vector<AbstractFeature<float>*>* features, std::vector<int32_t>* labels,
    bool feature_one_based, bool label_one_based, bool snappy_compressed) {
  petuum::HighResolutionTimer read_timer;
  features->resize(num_data);
  labels->resize(num_data);
  std::string file_str = snappy_compressed ?
    SnappyOpenFileToString(filename) : OpenFileToString(filename);
  std::istringstream data_stream(file_str);
  int32_t i = 0;
  std::vector<int32_t> feature_ids(feature_dim);
  std::vector<float> feature_vals(feature_dim);
  for (std::string line; std::getline(data_stream, line) && i < num_data;
      ++i) {
    int32_t label = ParseLibSVMLine(line, &feature_ids,
        &feature_vals, feature_one_based, label_one_based);
    (*labels)[i] = label;
    (*features)[i] = new SparseFeature<float>(feature_ids, feature_vals,
        feature_dim);
  }
  CHECK_EQ(num_data, i) << "Request to read " << num_data
    << " data instances but only " << i << " found in " << filename;
  LOG(INFO) << "Read " << i << " instances from " << filename << " in "
    << read_timer.elapsed() << " seconds.";
}

void ReadDataLabelLibSVM(const std::string& filename,
    int32_t feature_dim, int32_t num_data,
    std::vector<std::vector<float> >* features, std::vector<int32_t>* labels,
    bool feature_one_based, bool label_one_based, bool snappy_compressed) {
  petuum::HighResolutionTimer read_timer;
  features->resize(num_data);
  labels->resize(num_data);
  std::string file_str = snappy_compressed ?
    SnappyOpenFileToString(filename) : OpenFileToString(filename);
  std::istringstream data_stream(file_str);
  std::vector<int32_t> feature_ids(feature_dim);
  std::vector<float> feature_vals(feature_dim);
  int i = 0;
  for (std::string line; std::getline(data_stream, line) && i < num_data;
      ++i) {
    int32_t label = ParseLibSVMLine(line, &feature_ids,
        &feature_vals, feature_one_based, label_one_based);
    (*labels)[i] = label;
    (*features)[i].resize(feature_dim);
    for (int j = 0; j < feature_ids.size(); ++j) {
      (*features)[i][feature_ids[j]] = feature_vals[j];
    }
  }
  CHECK_EQ(num_data, i) << "Request to read " << num_data
    << " data instances but only " << i << " found in " << filename;
  LOG(INFO) << "Read " << i << " instances from " << filename << " in "
    << read_timer.elapsed() << " seconds.";
}

}  // namespace ml
}  // namespace petuum
