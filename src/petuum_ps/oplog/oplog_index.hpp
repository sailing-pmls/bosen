#pragma once

#include <vector>
#include <libcuckoo/cuckoohash_map.hh>
#include <boost/unordered_map.hpp>
#include <stdint.h>
#include <boost/noncopyable.hpp>

#include "petuum_ps/util/lock.hpp"
#include "petuum_ps/util/striped_lock.hpp"
#include "petuum_ps/thread/context.hpp"

namespace petuum {
class PartitionOpLogIndex : boost::noncopyable {
public:
  explicit PartitionOpLogIndex(size_t capacity);
  PartitionOpLogIndex(PartitionOpLogIndex && other);
  ~PartitionOpLogIndex();
  void AddIndex(const boost::unordered_map<int32_t, bool> &oplog_index);
  cuckoohash_map<int32_t, bool> *Reset();
private:
  size_t capacity_;
  SharedMutex smtx_;
  StripedLock<int32_t> locks_;
  cuckoohash_map<int32_t, bool> *shared_oplog_index_;
};

class TableOpLogIndex : boost::noncopyable{
public:
  explicit TableOpLogIndex(size_t capacity);
  void AddIndex(int32_t partition_num,
                const boost::unordered_map<int32_t, bool> &oplog_index);
  cuckoohash_map<int32_t, bool> *ResetPartition(int32_t partition_num);
private:
  std::vector<PartitionOpLogIndex> partition_oplog_index_;
};
}
