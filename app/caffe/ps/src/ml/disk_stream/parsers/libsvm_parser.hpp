// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.11.22

#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <ml/feature/abstract_feature.hpp>
#include <ml/feature/abstract_datum.hpp>
#include <ml/feature/sparse_feature.hpp>
#include <ml/feature/dense_feature.hpp>
#include <ml/disk_stream/parsers/abstract_parser.hpp>
#include <ml/disk_stream/data_parser_common.hpp>

namespace petuum {
namespace ml {

namespace {

const int32_t kBase = 10;

}  // anonymous namespace

struct LibSVMParserConfig {
  OutputFeatureType output_feature_type = kSparseFeature;
  int32_t feature_dim = 0;
  bool feature_one_based = false;
  bool label_one_based = false;
};

// Read LibSVM data format to AbstractDatum<V>.
template<typename V = float>
class LibSVMParser : public AbstractParser<AbstractDatum<V> > {
public:
  LibSVMParser(const LibSVMParserConfig& config) :
    output_feature_type_(config.output_feature_type),
    feature_dim_(config.feature_dim),
    feature_one_based_(config.feature_one_based),
    label_one_based_(config.label_one_based) { }

  // Implement AbstractParser interface.
  AbstractDatum<V>* Parse(char const* line, int* num_bytes_used) {
    std::vector<int32_t> feature_ids;
    std::vector<V> feature_vals;
    char *ptr = 0, *endptr = 0;
    if (line[0] == '\n') {
      // No more buffer to read.
      return 0;
    }
    // Read label.
    int label = strtol(line, &endptr, kBase);
    label = label_one_based_ ? label - 1 : label;
    ptr = endptr;

    while (isspace(*ptr) && (*ptr != '\n')) ++ptr;
    while (*ptr != '\n') {
      // read a feature_id:feature_val pair
      int32_t feature_id = strtol(ptr, &endptr, kBase);
      if (feature_one_based_) {
        --feature_id;
      }
      feature_ids.push_back(feature_id);
      ptr = endptr;
      CHECK_EQ(':', *ptr);
      ++ptr;

      // Comment (wdai): double will get converted to int if needed.
      feature_vals.push_back(strtod(ptr, &endptr));
      ptr = endptr;
      while (isspace(*ptr) && (*ptr != '\n')) ++ptr;
    }
    AbstractFeature<V>* feature;
    if (output_feature_type_ == kSparseFeature) {
      feature = new SparseFeature<V>(feature_ids, feature_vals,
          feature_dim_);
    } else {
      feature = new DenseFeature<V>(feature_ids, feature_vals,
          feature_dim_);
    }
    *num_bytes_used = ptr + 1 - line;  // +1 to get over '\n' character.
    return new AbstractDatum<V>(feature, label);
  }

private:
  OutputFeatureType output_feature_type_;
  int32_t feature_dim_;
  bool feature_one_based_;
  bool label_one_based_;
};


}  // namespace ml
}  // namespace petuum
