#ifndef _TOPIC_COUNT_HPP_
#define _TOPIC_COUNT_HPP_
#include <vector>
#include <mutex>
#include <atomic>
#include <memory>
struct Pair {
  int top_, cnt_;
  Pair() = default;
  Pair(int top, int cnt) : top_(top), cnt_(cnt) {}
};

struct TopicCount {
public:
  std::vector<Pair> item_;
public:
  TopicCount() = default;
  TopicCount(const char *bytes, int num_item);
  void AddCount(int topic, int doc_id, std::unique_ptr<std::mutex[]> &mutex_pool_);
  void UpdateCount(int old_topic, int new_topic, int doc_id, std::unique_ptr<std::mutex[]> &mutex_pool_);
  void Print(); // debug
private:
  void IncrementExisting(int index);
};

#endif 
