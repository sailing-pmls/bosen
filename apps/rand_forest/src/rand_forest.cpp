// Author: Jiesi Zhao (jiesizhao0423@gmail.com), Wei Dai (wdai@cs.cmu.edu)
// Date: 2014.11.8

#include "rand_forest.hpp"
#include <petuum_ps_common/include/petuum_ps.hpp>


namespace tree {

RandForest::RandForest(const RandForestConfig& config) :
  client_id_(config.client_id), thread_id_(config.thread_id),
  num_threads_(config.num_threads), num_trees_(config.num_trees),
  num_labels_(config.tree_config.num_labels),
  save_trees_(config.save_trees), trees_(config.num_trees), tree_config_(config.tree_config) { 

    for (int i = 0; i < num_trees_; ++i) {
      trees_[i] = new DecisionTree();
    }
  }

void RandForest::Train() {
    petuum::HighResolutionTimer train_timer;
    for (int i = 0; i < num_trees_; ++i) {
      // Build each tree.
      trees_[i]->Init(tree_config_);
      if (save_trees_) {
        serial_trees_.push_back(trees_[i]->GetSerializedTree());
      }
      if ((client_id_ == 0) && (thread_id_ == 0)) {
        LOG_EVERY_N(INFO, 10) << "Thread 0 on client 0 trained " 
          << i + 1 << "/" << num_trees_ << " trees. Time: "
          << train_timer.elapsed();
      }
    }
  }

int32_t RandForest::Predict(const petuum::ml::AbstractFeature<float>& x,
  std::vector<int32_t>* votes) const {
    std::vector<int32_t> votes_copy(num_labels_);
    for (int i = 0; i < num_trees_; ++i) {
      int32_t pred_label = trees_[i]->Predict(x);
      ++votes_copy[pred_label];
    }

    int32_t max_label = 0;
    int32_t max_vote = votes_copy[0];
    for (int j = 1; j < num_labels_; ++j) {
      if (votes_copy[j] > max_vote) {
        max_label = j;
        max_vote = votes_copy[j];
      }
    }
    if (votes != 0) {
      *votes = votes_copy;
    }
    return max_label;
}

void RandForest::SaveTrees(std::string output_file) {
  std::ofstream fout;
  fout.open(output_file, std::ios::app);
  CHECK(fout != NULL) << "Cannot open output file.";
  for (int i = 0; i < serial_trees_.size(); ++i) {
    fout << serial_trees_[i] << std::endl;
  }
  fout.close();
}

void RandForest::LoadTrees(std::string input_file) {
  std::ifstream fin;
  fin.open(input_file);
  std::string str;
  CHECK(fin != NULL) << "Cannot open input file.";

  num_trees_ = 0;
  while (getline(fin, str)) {
    num_trees_ ++;
    DecisionTree* tree = new DecisionTree(str);
    trees_.push_back(tree);
  }

  CHECK(num_trees_) << "Do not load any trees from file!";
  fin.close();
}


RandForest::~RandForest() {
  for (int i = 0; i < trees_.size(); ++i) {
    delete trees_[i];
  }
}


}  // namespace tree
