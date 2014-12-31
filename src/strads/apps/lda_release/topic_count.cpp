#include "topic_count.hpp"
#include <glog/logging.h>
#include <stdio.h>
#include <algorithm>

TopicCount::TopicCount(const char *bytes, int num_item) : item_(num_item) {
  memcpy(item_.data(), bytes, sizeof(Pair) * num_item);
}

void TopicCount::AddCount(int topic, int doc_id, std::unique_ptr<std::mutex[]> &mutex_pool_) { // doc id is local id within a process
  std::lock_guard<std::mutex>lock(mutex_pool_[doc_id]);
  int index = -1;
  for (size_t i = 0; i < item_.size(); ++i) {
    if (item_[i].top_ == topic) index = i;
  }
  if (index != -1) IncrementExisting(index);
  else item_.emplace_back(topic, 1);
}

void TopicCount::UpdateCount(int old_topic, int new_topic, int doc_id, std::unique_ptr<std::mutex[]> &mutex_pool_) {
  std::lock_guard<std::mutex>lock(mutex_pool_[doc_id]);
  if (old_topic == new_topic) return;
  // Find old and new index
  int old_index = -1;
  int new_index = -1;
  for (size_t i = 0; i < item_.size(); ++i) {
    if (item_[i].top_ == old_topic) old_index = i;
    if (item_[i].top_ == new_topic) new_index = i;
    if (old_index != -1 and new_index != -1) break; // early stop
  }
  CHECK_NE(old_index, -1);
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

void TopicCount::Print() {
  for (const auto pair : item_)
    printf(" %d:%d", pair.top_, pair.cnt_);
  printf("\n");
}

void TopicCount::IncrementExisting(int index) {
  //  std::lock_guard<std::mutex>lock(mlock);
  //  std::lock_guard<std::mutex>lock(mutex_pool_[doc_id]);
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
