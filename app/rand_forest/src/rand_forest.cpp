// Author: Jiesi Zhao (jiesizhao0423@gmail.com), Wei Dai (wdai@cs.cmu.edu)
// Date: 2014.11.8

#include "rand_forest.hpp"
#include "io/general_fstream.hpp"
#include <mutex>
static std::mutex hdfs_write_mutex;
namespace tree {

RandForest::RandForest(const RandForestConfig& config) :
  client_id_(config.client_id), thread_id_(config.thread_id),
  num_threads_(config.num_threads), num_trees_(config.num_trees),
  num_labels_(config.tree_config.num_labels),
  save_trees_(config.save_trees), tree_config_(config.tree_config) { 

  }

void RandForest::Train() {

    LOG(INFO) <<"Client " << client_id_<< " Thread " << thread_id_ 
      << ": Random Forest begin training.";
    for (int i = 0; i < num_trees_; ++i) {
      // Build each tree.
      DecisionTree* tree = new DecisionTree();
      tree->Init(tree_config_);
      trees_.push_back(tree);
      if (save_trees_) {
        serial_trees_.push_back(trees_[i]->GetSerializedTree());        
      }
      if ((client_id_ == 0) && (thread_id_ == 0)) {
        LOG(INFO) << "Each thread trained " 
          << i + 1 << "/" << num_trees_ << " trees.";
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
  hdfs_write_mutex.lock();
  // std::ofstream fout;
  // fout.open(output_file, std::ios::app);
  std::cout << "SaveTrees to " << output_file << std::endl;
  petuum::io::ofstream fout(output_file, std::ios::app);
  CHECK(fout != NULL) << "Cannot open output file.";
  for (int i = 0; i < serial_trees_.size(); ++i) {
    fout << serial_trees_[i] << std::endl;
  }
  fout.close();
  hdfs_write_mutex.unlock();
}

void RandForest::LoadTrees(std::string input_file) {
  // std::ifstream fin;
  // fin.open(input_file);
  // std::cout << "LoadTrees from " << input_file << std::endl;
  petuum::io::ifstream fin(input_file);
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

}  // namespace tree
