#include "topic_count.hpp"

//#include <glog/logging.h>

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <sstream>

void TopicCount::InitFromString(std::string s) {
  std::stringstream ss(s);
  int k;
  while (ss >> k) AddCount(k);
}

void TopicCount::SerializeTo(std::string& bytes) {
  bytes.assign((char *)item_.data(), sizeof(Pair) * item_.size());
}

void TopicCount::DeSerialize(const std::string& bytes) {
  item_.clear();
  item_.resize(bytes.size() / sizeof(Pair));
  memcpy(item_.data(), bytes.c_str(), bytes.size());
}

void TopicCount::AddCount(int topic) {
  int index = -1;
  for (size_t i = 0; i < item_.size(); ++i) {
    if (item_[i].top_ == topic) index = i;
  }
  if (index != -1) IncrementExisting(index);
  else item_.emplace_back(topic, 1);
}

void TopicCount::UpdateCount(int old_topic, int new_topic) {
  if (old_topic == new_topic) return;

  // Find old and new index
  int old_index = -1;
  int new_index = -1;
  for (size_t i = 0; i < item_.size(); ++i) {
    if (item_[i].top_ == old_topic) old_index = i;
    if (item_[i].top_ == new_topic) new_index = i;
    if (old_index != -1 and new_index != -1) break; // early stop
  }
  //CHECK_NE(old_index, -1);

  // Decrement old index while maintaining new index
  Pair temp(item_[old_index].top_, item_[old_index].cnt_ - 1);
  int last_index = item_.size() - 1;
  if (item_[old_index].cnt_ == 1) {
    if (new_index == last_index) new_index = old_index; // maintain
    std::swap(item_[old_index], item_.back()); // swap with last/self
    item_.pop_back();
  } // end of "need to shrink"
  else if (old_index < last_index and temp.cnt_ < item_[old_index+1].cnt_) {
    auto it = std::lower_bound(item_.begin() + old_index + 1,
                               item_.end(),
                               temp,
                               [](Pair a, Pair b){ return a.cnt_ > b.cnt_; })-1;
    if (item_.begin() + new_index == it) new_index = old_index; // maintain
    item_[old_index] = *it;
    *it = temp;
  } // end of "no need to shrink but need to rearrange"
  else {
    --item_[old_index].cnt_;
  } // end of "no need to rearrange"
    
  // Increment new
  if (new_index != -1) IncrementExisting(new_index);
  else item_.emplace_back(new_topic, 1);
}

void TopicCount::AddCountMap(count_map_t& count_map) {
  for (auto ind = item_.begin(); ind != item_.end(); ++ind) {
    auto it = count_map.find(ind->top_);
    if (it == count_map.end()) continue;
    ind->cnt_ += it->second;
    if (ind->cnt_ == 0) {
      std::swap(*ind, item_.back());
      item_.pop_back();
      --ind;
    }
    count_map.erase(it);
  }
  for (auto kv : count_map) item_.emplace_back(kv.first, kv.second);
  std::sort(item_.begin(),
            item_.end(),
            [](Pair a, Pair b){ return a.cnt_ > b.cnt_; });
}

void TopicCount::operator+=(const TopicCount& tc) {
  count_map_t cm;
  tc.ConvertToMap(&cm);
  AddCountMap(cm);
}

void TopicCount::operator-=(const TopicCount& tc) {
  count_map_t cm;
  tc.ConvertToMap(&cm, -1);
  AddCountMap(cm);
}

int TopicCount::ConvertToMap(count_map_t *out, int mult) const {
  out->clear();
  int sum = 0;
  for (auto pair : item_) {
    int count = mult * pair.cnt_;
    (*out)[pair.top_] = count;
    sum += count;
  }
  return sum;
}

std::string TopicCount::Print() {
  std::string s;
  for (const auto pair : item_)
    s += std::to_string(pair.top_) + ':' + std::to_string(pair.cnt_) + ' ';
  s += '\n';
  return s;
}

void TopicCount::IncrementExisting(int index) {
  Pair temp(item_[index].top_, item_[index].cnt_ + 1);
  if (index > 0 and temp.cnt_ > item_[index-1].cnt_) {
    auto it = std::upper_bound(item_.begin(),
                               item_.begin() + index,
                               temp,
                               [](Pair a, Pair b){ return a.cnt_ > b.cnt_; });
    item_[index] = *it;
    *it = temp;
  } // end of "need to rearrange"
  else {
    ++item_[index].cnt_;
  } // end of "no need to rearrange"
}

