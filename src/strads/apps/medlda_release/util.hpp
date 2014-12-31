#ifndef UTIL_HPP
#define UTIL_HPP

#include <glog/logging.h>
#include <gflags/gflags.h>

#include <time.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <fstream>

// Google Log hack
#define LR LOG(INFO)<<"(rank:"<<ctx_->rank<<") "

// Container hack
#define RANGE(x) ((x).begin()), ((x).end())
#define SUM(x)   (std::accumulate(RANGE(x), .0))

// Random hack
#include <chrono>
#include <random>
#define CLOCK (std::chrono::system_clock::now().time_since_epoch().count())
static std::mt19937 _rng(CLOCK);
static std::uniform_real_distribution<double> _unif01;
static std::normal_distribution<double> _stdnormal;

struct ThreadRNG {
  ThreadRNG() : rng_(CLOCK) {}
  std::mt19937 rng_;
  std::uniform_real_distribution<double> unif01_;
  std::normal_distribution<double> stdnormal_;
};

// Eigen
#define EIGEN_INITIALIZE_MATRICES_BY_ZERO
#define EIGEN_DEFAULT_IO_FORMAT \
        Eigen::IOFormat(StreamPrecision,1," "," ","","","[","]")
#include <Eigen/Dense>
using EMatrix = Eigen::MatrixXd;
using EVector = Eigen::VectorXd;
using EArray  = Eigen::ArrayXd;
using EMAtrix = Eigen::ArrayXXd;
using EMatrixMap = Eigen::Map<EMatrix>;
using EVectorMap = Eigen::Map<EVector>;
using EArrayMap  = Eigen::Map<EArray>;

template<typename T>
using TMatrix = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>;

template<typename T>
static void save_matrix(const TMatrix<T>& mat, std::string file) {
  std::ofstream fout(file); CHECK(fout.good());
  Eigen::IOFormat space_sep(Eigen::StreamPrecision,0," ","\n","","","","");
  fout << mat.format(space_sep);
  fout.close();
}

template<typename T>
static void load_matrix(TMatrix<T> *mat, std::string file) {
  std::ifstream fin(file); CHECK(fin.good());
  int nrows = 0;
  std::string line;
  while (std::getline(fin, line)) ++nrows;
  fin.clear(); fin.seekg(0); // rewind
  T val;
  std::vector<T> flat; // row major
  while (not fin.eof()) {
    fin >> val;
    flat.push_back(val);
  }
  fin.close();
  mat->resize(flat.size() / nrows, nrows);
  memcpy(mat->data(), flat.data(), sizeof(double) * flat.size());
  mat->transposeInPlace();
}

template<typename T>
static void rowwise_max_ind(const TMatrix<T>& mat, TMatrix<int> *ind) {
  int nrows = mat.rows();
  ind->resize(nrows, 1);
  for (int i = 0; i < nrows; ++i) {
    mat.row(i).maxCoeff(ind->data() + i);
  }
}

// Multivariate Normal with mean = covariance * v; covariance = inv(precision)
static EVector draw_mvgaussian(const EMatrix& precision, const EVector& v) {
  Eigen::LLT<EMatrix> chol;
  chol.compute(precision);
  EVector alpha = chol.matrixL().solve(v); // alpha = L' * mu
  EVector mu = chol.matrixU().solve(alpha);
  EVector z(v.size());
  for (int i = 0; i < v.size(); ++i)
    z(i) = _stdnormal(_rng);
  EVector x = chol.matrixU().solve(z);
  return mu + x;
}

// Get monotonic time in seconds from a starting point
static double get_time() {
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC, &start);
  return (start.tv_sec + start.tv_nsec/1000000000.0);
}

class Timer {
public:
  void   tic() { start_ = get_time(); }
  double toc() { double ret = get_time() - start_; time_ += ret; return ret; }
  double get() { return time_; }
private:
  double time_  = .0;
  double start_ = get_time();
};

// Google flags hack
static void print_help() {
  fprintf(stderr, "Program Flags:\n");
  std::vector<google::CommandLineFlagInfo> all_flags;
  google::GetAllFlags(&all_flags);
  for (const auto& flag : all_flags) {
    if (flag.filename.find("src/") != 0) // HACK: filter out built-in flags
      fprintf(stderr,
              "-%s: %s (%s, default:%s)\n",
              flag.name.c_str(),
              flag.description.c_str(),
              flag.type.c_str(),
              flag.default_value.c_str());
  }
  exit(1);
}

// Google flags hack
static void print_flags() {
  LOG(INFO) << "---------------------------------------------------------------------";
  std::vector<google::CommandLineFlagInfo> all_flags;
  google::GetAllFlags(&all_flags);
  for (const auto& flag : all_flags) {
    if (flag.filename.find("src/") != 0) // HACK: filter out built-in flags
      LOG(INFO) << flag.name << ": " << flag.current_value;
  }
  LOG(INFO) << "---------------------------------------------------------------------";
}

// Faster strtol without error checking.
static long int strtol(const char *nptr, char **endptr) {
  // Skip spaces
  while (isspace(*nptr)) ++nptr;
  // Sign
  bool is_negative = false;
  if (*nptr == '-') {
    is_negative = true;
    ++nptr;
  } else if (*nptr == '+') {
    ++nptr;
  }
  // Go!
  long int res = 0;
  while (isdigit(*nptr)) {
    res = (res * 10) + (*nptr - '0');
    ++nptr;
  }
  if (endptr != NULL) *endptr = (char *)nptr;
  if (is_negative) return -res;
  return res;
}

#endif
