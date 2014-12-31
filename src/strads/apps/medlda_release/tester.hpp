#ifndef TESTER_HPP
#define TESTER_HPP

#include "dict.hpp"
#include "util.hpp"
#include "stat.hpp"
#include "strads/include/common.hpp"

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>

class Tester {
public:
  Tester(sharedctx* ctx);
  void LoadModel(std::string dump_prefix);
  void ReadData(std::string test_prefix);
  void Infer();
  void SaveResult(std::string dump_prefix);

private:
  void InferOneSample(int doc_id, int tid);

private:
  std::vector<Sample> test_; // test documents
  EMatrix classifier_; // K x L, each col is a classifier
  EMatrix model_; // K x V, each row is a distribution on words
  EMatrix theta_; // D x K
  std::mutex mtx_; // Not sure if EMatrix rowwise write is thread safe
  Dict dict_; // complete dict
  std::vector<ThreadRNG> trng_;
  std::vector<std::thread> thr_;
  std::atomic<int> task_id_;
  std::atomic<int> acc_;
  sharedctx *ctx_;
  Timer timer_;

  // hyperparams read from dump
  double alpha_, beta_;
  int num_topic_, num_label_;
};

#endif
