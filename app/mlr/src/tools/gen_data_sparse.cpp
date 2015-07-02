// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.10.11
//
// Description: Generate multi/binary-class synthetic data from correlation
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


// Generate dataset and output in libsvm (transpose) format.

DEFINE_int32(num_train, 10000, "# of training data.");
DEFINE_int32(feature_dim, 2000, "feature dimension.");
DEFINE_string(output_file, " ", "Generate "
    "output_file, output_file.meta");
DEFINE_int32(num_partitions, 1, "# of output files. Output files are"
    "output_file.0, output_file.1 ...");
DEFINE_int32(nnz_per_col, 25, "# of non-zeros in each column.");
DEFINE_bool(one_based, false, "feature index starts at 1.");
DEFINE_double(correlation_strength, 0.9, "correlation strength in [0, 1].");
DEFINE_bool(snappy_compressed, false, "compress with snappy.");
DEFINE_double(noise_ratio, 0.1, "With this probability an instance label is "
    "randomly assigned. Only used in classification.");
DEFINE_double(beta_sparsity, 0.01, "nnz in ground-truth beta / feature_dim");
DEFINE_int32(num_labels, 2, "# of (balanced) classes.");

namespace {

// Sample t items from {0, 1, ..., n - 1} without replacement.
std::vector<int> SampleWithoutReplacement(int n, int t) {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  std::uniform_int_distribution<> unif(0, n - 1);
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

template <typename T>
std::vector<size_t> SortIndex(const std::vector<T> &v) {

  // initialize original index locations
  std::vector<size_t> idx(v.size());
  for (size_t i = 0; i != idx.size(); ++i) idx[i] = i;

  // sort indexes based on comparing values in v
  sort(idx.begin(), idx.end(),
      [&v](size_t i1, size_t i2) {return v[i1] < v[i2];});

  return idx;
}

class SparseFeature {
  public:
    SparseFeature() : ids_(default_size_), vals_(default_size_), nnz_(0) { }
    SparseFeature(int size) : ids_(size), vals_(size), nnz_(0) { }

    static void SetDefaultSize(int size) {
      default_size_ = size;
    }

    void PushBack(int id, float val) {
      if (nnz_ == ids_.size()) {
        ids_.resize(ids_.size() * 2);
        vals_.resize(vals_.size() * 2);
      }
      ids_[nnz_] = id;
      vals_[nnz_] = val;
      ++nnz_;
    }

    const std::vector<int>& GetFeatureIds() {
      ids_.resize(nnz_);
      return ids_;
    }

    const std::vector<float>& GetFeatureVals() {
      vals_.resize(nnz_);
      return vals_;
    }

  public:
    static int default_size_;

  private:
    std::vector<int> ids_;
    std::vector<float> vals_;
    int nnz_;   // # of non-zeros in the sparse feature.
};

int SparseFeature::default_size_ = 10;

float SparseDotProduct(const std::vector<float>& beta,
    SparseFeature& x) {
  float sum = 0.;
  const std::vector<int>& ids = x.GetFeatureIds();
  const std::vector<float>& vals = x.GetFeatureVals();
  for (int i = 0; i < ids.size(); ++i) {
    int feature_id = ids[i];
    sum += beta[feature_id] * vals[i];
  }
  return sum;
}


void GenerateMetaFile(const std::string& filename,
    int num_train_this_partition, int beta_nnz) {
  // Generate meta file
  std::ofstream meta_stream(filename + ".meta", std::ofstream::out
      | std::ofstream::trunc);
  meta_stream << "num_train_total: " << FLAGS_num_train << std::endl;
  meta_stream << "num_train: " << FLAGS_num_train << std::endl;
  meta_stream << "num_train_this_partition: "
    << num_train_this_partition << std::endl;
  meta_stream << "feature_dim: " << FLAGS_feature_dim << std::endl;
  meta_stream << "num_labels: " << FLAGS_num_labels << std::endl;
  meta_stream << "format: " << "libsvm"  << std::endl;
  meta_stream << "feature_one_based: " << FLAGS_one_based << std::endl;
  meta_stream << "label_one_based: " << false << std::endl;
  meta_stream << "nnz_per_column: " << FLAGS_nnz_per_col << std::endl;
  meta_stream << "correlation_strength: "
    << FLAGS_correlation_strength << std::endl;
  meta_stream << "snappy_compressed: " << FLAGS_snappy_compressed << std::endl;
  meta_stream << "sample_one_based: " << FLAGS_one_based << std::endl;
  meta_stream << "beta_nnz: " << beta_nnz << std::endl;
  meta_stream << "noise_ratio: " << FLAGS_noise_ratio << std::endl;
  LOG(INFO) << "Wrote meta data to " << filename + ".meta";
}

void Normalize(std::vector<float>* v) {
  float l2_norm = 0.;
  for (int k = 0; k < v->size(); ++k) {
    l2_norm += (*v)[k] * (*v)[k];
  }
  for (int k = 0; k < v->size(); ++k) {
    (*v)[k] /= l2_norm;
  }
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

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> unif_minus_plus_one(-1., 1.);

  // i indexs samples, j indexes feature dim, k indexes non-zero values.

  // Generate \beta.
  int64_t beta_nnz = FLAGS_feature_dim * FLAGS_beta_sparsity;
  LOG(INFO) << "beta_nnz: " << beta_nnz;
  //auto nnz_idx = SampleWithoutReplacement(FLAGS_feature_dim, beta_nnz);
  std::vector<int64_t> nnz_idx(beta_nnz);
  for (int j = 0; j < beta_nnz; ++j) {
    nnz_idx[j] = j;
  }
  std::vector<float> beta(FLAGS_feature_dim);
  for (int j = 0; j < nnz_idx.size(); ++j) {
    beta[nnz_idx[j]] = unif_minus_plus_one(gen);
  }
  {
    // Write beta file.
    std::stringstream ss;
    const auto sort_beta_nnz = SortIndex(nnz_idx);
    for (int k = 0; k < sort_beta_nnz.size(); ++k) {
      int64_t idx = nnz_idx[sort_beta_nnz[k]];
      ss << idx << " " << beta[idx] << std::endl;
    }
    std::string filename = FLAGS_output_file + "x"
      + std::to_string(FLAGS_num_partitions) + ".libsvm.beta";
    std::ofstream beta_out(filename);
    auto out_str = ss.str();
    beta_out.write(out_str.data(), out_str.size());
    LOG(INFO) << "Wrote beta to " << filename << " (" << out_str.size()
      << " bytes)";
  }

  // X[i] is the ith sample. map is 0-based.

  // 0 <= sample_idx[k] < FLAGS_num_train. sample_idx[k] is the sample idx that
  // will get the k-th generated value. This gets shuffle with 10% probability
  // going to the next feature.
  // Generate first column by picking some samples to be non-zeros.
  std::vector<int> sample_idx =
    SampleWithoutReplacement(FLAGS_num_train, FLAGS_nnz_per_col);

  // Previous values. Never shuffled.
  std::vector<float> prev_val(FLAGS_nnz_per_col);

  // Generate the first column.
  for (int k = 0; k < FLAGS_nnz_per_col; ++k) {
    prev_val[k] = unif_minus_plus_one(gen);
  }
  Normalize(&prev_val);

  int64_t est_nnz_per_sample = std::ceil(
      static_cast<float>(FLAGS_nnz_per_col)
      * FLAGS_feature_dim / FLAGS_num_train) * 1.1;
  SparseFeature::SetDefaultSize(est_nnz_per_sample);
  std::vector<SparseFeature> X(FLAGS_num_train);

  for (int j = 0; j < FLAGS_feature_dim; ++j) {
    LOG_EVERY_N(INFO, 10000) << "Progress: " << j
      << " / " << FLAGS_feature_dim << " ("
      << static_cast<float>(j) / FLAGS_feature_dim * 100 << "%)";

    // Write curr col to X.
    for (int k = 0; k < FLAGS_nnz_per_col; ++k) {
      int sample_k = sample_idx[k];
      X[sample_k].PushBack(j, prev_val[k]);
    }

    // Generating the next col.
    if (unif_minus_plus_one(gen) >= FLAGS_correlation_strength) {
      // shuffle (no correlation).
      sample_idx =
        SampleWithoutReplacement(FLAGS_num_train, FLAGS_nnz_per_col);
    }
    // Normalize to control Lipschitz constant.
    for (int k = 0; k < FLAGS_nnz_per_col; ++k) {
      // add to existing value as corelation.
      prev_val[k] += unif_minus_plus_one(gen);
    }
    Normalize(&prev_val);
  }
  LOG(INFO) << "Done generating design matrix.";

  // compute the regression value Y.
  std::vector<float> Y(FLAGS_num_train);
  for (int i = 0; i < FLAGS_num_train; ++i) {
    Y[i] = SparseDotProduct(beta, X[i]);
  }

  // Compute the class labels from Y.
  std::vector<float> Y_sorted = Y;
  std::sort(Y_sorted.begin(), Y_sorted.end());
  // Find quantiles
  std::vector<float> quantiles(FLAGS_num_labels);
  for (int l = 0; l < FLAGS_num_labels; ++l) {
    quantiles[l] = (l == FLAGS_num_labels - 1) ?
      Y_sorted[Y_sorted.size() - 1] :
      Y_sorted[(l + 1) * FLAGS_num_train / FLAGS_num_labels];
  }
  std::vector<int32_t> labels(FLAGS_num_train);
  std::uniform_int_distribution<> class_rand(0, FLAGS_num_labels - 1);
  std::uniform_real_distribution<float> unif_zero_one(0., 1.);
  for (int i = 0; i < FLAGS_num_train; ++i) {
    if (unif_zero_one(gen) < FLAGS_noise_ratio) {
      // Noise label.
      labels[i] = class_rand(gen);
    } else {
      // Linearly separable labeling.
      labels[i] = FindQuantile(quantiles, Y[i]);
    }
  }

  LOG(INFO) << "Generated " << FLAGS_num_train << " data. Time: "
    << total_timer.elapsed();

  int num_train_per_partition = static_cast<float>(FLAGS_num_train) / 
    FLAGS_num_partitions;
  for (int ipar = 0; ipar < FLAGS_num_partitions; ++ipar) {
    int sample_begin = ipar * num_train_per_partition;
    int sample_end = (ipar == FLAGS_num_partitions - 1) ?
      FLAGS_num_train : sample_begin + num_train_per_partition;
    int num_train_this_partition = sample_end - sample_begin;
    // Write X file for this partition.
    std::string filename = FLAGS_output_file + ".x"
      + std::to_string(FLAGS_num_partitions) + ".libsvm.X."
      + std::to_string(ipar);
    FILE *outfile = fopen(filename.c_str(),"w");

    for (int i = sample_begin; i < sample_end; ++i) {
      const std::vector<int>& ids = X[i].GetFeatureIds();
      const std::vector<float>& vals = X[i].GetFeatureVals();
      fprintf(outfile, "%d ", labels[i]);
      for (int k = 0; k < ids.size(); ++k) {
        int sample_id = FLAGS_one_based ? ids[k] + 1 : ids[k];
        fprintf(outfile, "%d:%.4f ", sample_id, vals[k]);
      }
      fprintf(outfile, "\n");
    }
    fclose(outfile);
    GenerateMetaFile(filename, num_train_this_partition, beta_nnz);

    LOG(INFO) << "Wrote " << num_train_this_partition
      << " samples to file " << filename;
  }
  return 0;
}
