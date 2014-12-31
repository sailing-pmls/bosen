// author: jinliang
#pragma once

#include <boost/noncopyable.hpp>

namespace petuum {
class OpLogSerializer : boost::noncopyable {
public:
  OpLogSerializer() {}
  ~OpLogSerializer() {}

  size_t Init(const std::map<int32_t, size_t> &table_size_map) {
    num_tables_ = table_size_map.size();
    if (num_tables_ == 0)
      return 0;

    // space for num of tables
    size_t total_size = sizeof(int32_t);
    for(auto iter = table_size_map.cbegin(); iter != table_size_map.cend();
      iter++){
      int32_t table_id = iter->first;
      size_t table_size = iter->second;
      offset_map_[table_id] = total_size;
      // next table is offset by
      // 1) the current table size and
      // 2) space for table id
      // 3) update size
      total_size += table_size + sizeof(int32_t) + sizeof(size_t);
    }
    return total_size;
  }

  // does not take ownership
  void AssignMem(void *mem) {
    mem_ = reinterpret_cast<uint8_t*>(mem);
    *(reinterpret_cast<int32_t*>(mem_)) = num_tables_;
  }

  void *GetTablePtr(int32_t table_id) {
    auto iter = offset_map_.find(table_id);
    if (iter == offset_map_.end())
      return 0;
    return mem_ + iter->second;
  }

private:
  std::map<int32_t, size_t> offset_map_;
  uint8_t *mem_;
  int32_t num_tables_;
};

}
