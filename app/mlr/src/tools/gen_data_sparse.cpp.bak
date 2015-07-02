// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.10.11
//
// Description: Generate multi-class synthetic data from correlation
// mechanism and linear separator. The output is in libsvm format.
//
// Specifically, we start from first column, generate non zero values for
// `nnz_per_col` randomly selected rows. Going to the second column, with
// probability `correlation_strength` we use the same set of rows to have
// non-zero values at the second column; otherwise we randomly pick another
// set of `nnz_per_col` rows. After the rows are chosen, we generate
// `nnz_per_col` feature values by adding uniform rand numbers to each of the
// `nnz_per_col` feature values in column 1. Repeat for column 3 etc.
//
// After the features are generated, we assign labels based on the regression
// value of each data with respect to a randomly generated linear regressor.
// To have 10 classes, the data with the top 10% highest regressed value
// assigned label 1, then 10-20% are assigned label 2, etc.

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <petuum_ps_common/include/petuum_ps.hpp>
#include <fstream>
#include <cstdio>
#include <map>
#include <string>
#include <algorithm>
#include <sstream>
#include <cmath>
#include <random>
#include <cstdint>
#include <snappy.h>

DEFINE_int32(num_train, 10000, "# of training data.");
DEFINE_int32(feature_dim, 2000, "feature dimension.");
DEFINE_int32(num_labels, 2, "# of classes.");
DEFINE_string(output_file, " ", "Generate "
    "output_file, output_file.meta");
DEFINE_int32(nnz_per_col, 25, "# of non-zeros in each column.");
DEFINE_bool(one_based, false, "feature index starts at 1. Only relevant to "
    "libsvm format.");
DEFINE_double(correlation_strength, 0.9, "correlation strength in [0, 1].");
DEFINE_bool(snappy_compressed, true, "Compress output with snappy.");
DEFINE_double(noise_ratio, 0.1, "With this probability an instance label is "
    "randomly assigned.");

namespace {

// Sample t items from {0, 1, ..., n - 1} without replacement.
std::vector<int> SampleWithoutReplacement(int n, int t) {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<> unif(0, n - 1);
  std::vector<int> sample_idx(t, -1);
  for (int num_sampled = 0; num_sampled < t; ) {
    int sample = unif(gen);
    int repeat = false;
    for (int i = 0; i < num_sampled; ++i) {
      if (sample_idx[i] == sample) {
        repeat = true;
        break;
      }
    }
    if (!repeat) {
      sample_idx[num_sampled++] = sample;
    }
  }
  return sample_idx;
}

float SparseDotProduct(const std::vector<float>& beta,
    const std::map<int, float>& x) {
  float sum = 0.;
  for (const auto& p : x) {
    sum += beta[p.first] * p.second;
  }
  return sum;
}

// Quantile is the label (first quantile is label 0, etc).
int FindQuantile(const std::vector<float>& quantiles, float val) {
  for (int q = 0; q < FLAGS_num_labels; ++q) {
    if (val <= quantiles[q]) {
      return q;
    }
  }
  LOG(FATAL) << "value exceeds quantile: " << val;
  return 0;
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  petuum::HighResolutionTimer total_timer;

  // i indexs samples, j indexes feature dim, k indexes non-zero values.


  // X[i] is the ith sample. map is 0-based.
  std::vector<std::map<int, float> > X(FLAGS_num_train);

  // 0 <= sample_idx[k] < FLAGS_num_train. sample_idx[k] is the sample idx that
  // will get the k-th generated value. This gets shuffle with 10% probability
  // going to the next feature.
  // Generate first column by picking some samples to be non-zeros.
  std::vector<int> sample_idx =
    SampleWithoutReplacement(FLAGS_num_train, FLAGS_nnz_per_col);

  // Previous values. Never shuffled.
  std::vector<float> prev_val(FLAGS_nnz_per_col);

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> unif_minus_plus_one(-1., 1.);

  // Generate nnz_per_col values.
  for (int k = 0; k < FLAGS_nnz_per_col; ++k) {
    prev_val[k] = unif_minus_plus_one(gen);
    int sample_k = sample_idx[k];
    X[sample_k][0] = prev_val[k];
  }

  for (int j = 1; j < FLAGS_feature_dim; ++j) {
    LOG_EVERY_N(INFO, 100000) << "Progress: " << j << " / " << FLAGS_feature_dim;
    if (unif_minus_plus_one(gen) >= FLAGS_correlation_strength) {
      // shuffle (no correlation).
      sample_idx =
        SampleWithoutReplacement(FLAGS_num_train, FLAGS_nnz_per_col);
    }
    for (int k = 0; k < FLAGS_nnz_per_col; ++k) {
      prev_val[k] += unif_minus_plus_one(gen);   // add to existing value as corelation.
      int sample_k = sample_idx[k];
      X[sample_k][j] = prev_val[k];
    }
  }

  // Compute labels.
  // Generate \beta.
  std::vector<float> beta(FLAGS_feature_dim);
  for (int j = 0; j < FLAGS_feature_dim; ++j) {
    beta[j] = unif_minus_plus_one(gen);
  }

  std::vector<float> regress_val(FLAGS_num_train);
  for (int i = 0; i < FLAGS_num_train; ++i) {
    regress_val[i] = SparseDotProduct(beta, X[i]);
  }

  std::vector<float> regress_sorted = regress_val;
  std::sort(regress_sorted.begin(), regress_sorted.end());
  // Find quantiles
  std::vector<float> quantiles(FLAGS_num_labels);
  for (int l = 0; l < FLAGS_num_labels; ++l) {
    quantiles[l] = (l == FLAGS_num_labels - 1) ?
      regress_sorted[regress_sorted.size() - 1] :
      regress_sorted[(l + 1) * FLAGS_num_train / FLAGS_num_labels];
  }
  std::vector<int> labels(FLAGS_num_train);
  std::uniform_int_distribution<> class_rand(0, FLAGS_num_labels - 1);
  std::uniform_real_distribution<float> unif_zero_one(0., 1.);
  for (int i = 0; i < FLAGS_num_train; ++i) {
    if (unif_zero_one(gen) < FLAGS_noise_ratio) {
      // Noise label.
      labels[i] = class_rand(gen);
    } else {
      // Linearly separable labeling.
      labels[i] = FindQuantile(quantiles, regress_val[i]);
    }
  }

  // Write to file.
  if (!FLAGS_snappy_compressed) {
    FILE* outfile = fopen(FLAGS_output_file.c_str(), "w");
    for (int i = 0; i < FLAGS_num_train; ++i) {
      fprintf(outfile, "%d ", labels[i]);
      for (const auto& p : X[i]) {
        int feature_idx = FLAGS_one_based ? p.first + 1 : p.first;
        fprintf(outfile, "%d:%f ", feature_idx, p.second);
      }
      fprintf(outfile, "\n");
    }
    CHECK_EQ(0, fclose(outfile)) << "Failed to close file " << FLAGS_output_file;
  } else {
    std::stringstream ss;
    for (int i = 0; i < FLAGS_num_train; ++i) {
      ss << labels[i] << " ";
      for (const auto& p : X[i]) {
        int feature_idx = FLAGS_one_based ? p.first + 1 : p.first;
        ss << feature_idx << ":" << p.second << " ";
      }
      ss << std::endl;
    }
    // Snappy compress.
    std::string compressed_str;
    std::string original_str = ss.str();
    snappy::Compress(original_str.data(), original_str.size(), &compressed_str);
    std::ofstream outfile(FLAGS_output_file, std::ofstream::binary);
    outfile.write(compressed_str.data(), compressed_str.size());
  }

  // Generate meta file
  std::string meta_file = FLAGS_output_file + ".meta";
  std::ofstream meta_stream(meta_file, std::ofstream::out
      | std::ofstream::trunc);
  meta_stream << "num_train_total: " << FLAGS_num_train << std::endl;
  meta_stream << "num_train_this_partition: "
    << FLAGS_num_train << std::endl;
  meta_stream << "feature_dim: " << FLAGS_feature_dim << std::endl;
  meta_stream << "num_labels: " << FLAGS_num_labels << std::endl;
  meta_stream << "format: " << "libsvm" << std::endl;
  meta_stream << "feature_one_based: " << FLAGS_one_based << std::endl;
  meta_stream << "label_one_based: " << false << std::endl;
  meta_stream << "nnz_per_column: " << FLAGS_nnz_per_col << std::endl;
  meta_stream << "correlation_strength: " << FLAGS_correlation_strength << std::endl;
  meta_stream << "snappy_compressed: " << FLAGS_snappy_compressed << std::endl;
  meta_stream.close();

  LOG(INFO) << "Generated " << FLAGS_num_train << " data of dim "
    << FLAGS_feature_dim << " at " << FLAGS_output_file
    << " in " << total_timer.elapsed();

  return 0;
}
