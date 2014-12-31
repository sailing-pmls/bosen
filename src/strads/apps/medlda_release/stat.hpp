#ifndef STAT_HPP
#define STAT_HPP

#include "util.hpp"
#include "topic_count.hpp"

#include <mutex>
#include <memory>
#include <atomic>
#include <vector>

struct Sample {
  EArray label_; // L x 1, {+1,-1}
  EArray aux_; // L x 1, c^2 lambda^-1 in the paper
  std::vector<int> token_;
  std::vector<int> assignment_;
};

struct Stat {
public:
  int num_word_, num_topic_;
  std::vector<TopicCount> tc_; // word-topic counts
  std::unique_ptr<std::mutex[]> mutex_pool_; // mutex for doc topic
  std::unique_ptr<std::atomic<int>[]> summary_;

public:
  void Init(int num_word, int num_topic);
  void CopyFrom(const Stat& other);
  void InitFromDocs(const std::vector<Sample>& docs);

  void GetCount(int word_id, TopicCount& tc);
  void GetCountArray(int word_id, EArray& arr);
  void GetCountBytes(int word_id, std::string& bytes);
  void GetSummaryArray(EArray& arr);

  // my stat += (tc - old)
  void MergeFrom(int word_id, count_map_t& count_map);
  void UpdateWord(int word_id, int old_topic, int new_topic);

};

#endif
