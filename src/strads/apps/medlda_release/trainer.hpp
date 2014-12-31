#ifndef TRAINER_HPP
#define TRAINER_HPP

#include <mpi.h>
#include "dict.hpp"
#include "util.hpp"
#include "topic_count.hpp"
#include "stat.hpp"
#include "strads/include/common.hpp"
#include "medlda-ps.hpp"

#include <string>
#include <vector>
#include <atomic>
#include <thread>

class Trainer {
public:
  Trainer(sharedctx* ctx);

  // Expecting filename "prefix.rank", e.g. "data.0", "data.1"
  // libsvm format: [label][space][label][space]..[word]:[count][space]..
  // Assuming no trailing spaces.
  void ReadData(std::string train_prefix);

  // Parameter estimation on training data
  void Train();

  // Save doc topic estimate, predicted label, and trained model
  void SaveResult(std::string dump_prefix);

private:
  void InitParam();
  void TrainOneSample(Sample& doc, int tid);
  void AccumulateThread();
  void DrawClassifier();
  void PredictThread();
  void PrintResult();
  void Sync();

private:
  std::vector<Sample> train_; // train documents
  EMatrix classifier_; // K x L, each col is a classifier
  Stat  stat_, prev_stat_; // partial word-topic counts
  Dict  dict_; // partial dict
  Timer timer_;
  sharedctx *ctx_;
  Pointers ptrs_;
  std::vector<ThreadRNG> trng_;
  std::atomic<bool> stop_sync_;
  std::atomic<int> task_id_;
  std::atomic<int> acc_;
  std::vector<std::thread> thr_;

  // draw eta
  std::vector<EVector> sub_vec_; // size L
  std::vector<EMatrix> sub_prec_; // size L
  std::vector<int> label_map_; // label to client mapping
};

#endif
