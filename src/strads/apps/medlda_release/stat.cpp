#include "stat.hpp"

void Stat::Init(int nword, int ntopic) {
  num_word_ = nword;
  num_topic_ = ntopic;
  tc_.resize(num_word_);
  mutex_pool_.reset(new std::mutex[num_word_]);
  summary_.reset(new std::atomic<int>[num_topic_]);
  for (int k = 0; k < num_topic_; ++k) summary_[k] = 0;
}

void Stat::CopyFrom(const Stat& other) {
  num_word_  = other.num_word_;
  num_topic_ = other.num_topic_;
  tc_ = other.tc_;
  mutex_pool_.reset(new std::mutex[num_word_]);
  summary_.reset(new std::atomic<int>[num_topic_]);
  for (int k = 0; k < num_topic_; ++k) summary_[k] = (int)other.summary_[k];
}

void Stat::InitFromDocs(const std::vector<Sample>& docs) {
  for (const auto& doc : docs) {
    for (size_t n = 0; n < doc.token_.size(); ++n) {
      int topic = doc.assignment_[n];
      int word_id = doc.token_[n];
      tc_[word_id].AddCount(topic);
      ++summary_[topic];
    }
  }
}

void Stat::GetCount(int word_id, TopicCount& tc) {
  std::lock_guard<std::mutex> lock(mutex_pool_[word_id]);
  tc = tc_[word_id];
}

void Stat::GetCountArray(int word_id, EArray& arr) {
  std::lock_guard<std::mutex> lock(mutex_pool_[word_id]);
  for (auto pair : tc_[word_id].item_) arr(pair.top_) = pair.cnt_;
}

void Stat::GetCountBytes(int word_id, std::string& bytes) {
  std::lock_guard<std::mutex> lock(mutex_pool_[word_id]);
  tc_[word_id].SerializeTo(bytes);
}

void Stat::GetSummaryArray(EArray& arr) {
  arr.setZero(num_topic_);
  for (int k = 0; k < num_topic_; ++k) arr(k) = summary_[k];
}

void Stat::MergeFrom(int word_id, count_map_t& count_map) {
  for (auto kv : count_map) summary_[kv.first] += kv.second;
  std::lock_guard<std::mutex> lock(mutex_pool_[word_id]);
  tc_[word_id].AddCountMap(count_map);
}

void Stat::UpdateWord(int word_id, int old_topic, int new_topic) {
  --summary_[old_topic];
  ++summary_[new_topic];
  std::lock_guard<std::mutex> lock(mutex_pool_[word_id]);
  tc_[word_id].UpdateCount(old_topic, new_topic);
}

