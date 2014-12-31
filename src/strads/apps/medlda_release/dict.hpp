#ifndef DICT_HPP
#define DICT_HPP

#include <glog/logging.h>

#include <string>
#include <unordered_map>

// Mapping between word string and 0-based consecutive word id
class Dict {
public:
  // Add new entry, return id if exists
  int insert_word(std::string word) {
    auto it = word2id_.find(word);
    if (it != word2id_.end()) { return it->second; } // found
    else { word2id_[word] = cnt_; id2word_[cnt_] = word; return cnt_++; } // new
  }
  // Return negative if not exist
  int get_id(std::string word) {
    auto it = word2id_.find(word);
    if (it != word2id_.end()) { return it->second; } // found
    else { return -1; }
  }
  // Error if id is not found
  std::string get_word(int id) {
    auto it = id2word_.find(id);
    CHECK(it != id2word_.end());
    return it->second;
  }
  size_t size() { return cnt_; }

private:
  size_t cnt_ = 0;
  std::unordered_map<std::string, int> word2id_;
  std::unordered_map<int, std::string> id2word_;
};

#endif
