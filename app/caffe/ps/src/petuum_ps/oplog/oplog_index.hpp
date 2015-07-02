#pragma once

#include <vector>
#include <libcuckoo/cuckoohash_map.hh>
#include <unordered_set>
#include <stdint.h>
#include <boost/noncopyable.hpp>

#include <petuum_ps_common/util/lock.hpp>
#include <petuum_ps_common/util/striped_lock.hpp>
#include <petuum_ps/thread/context.hpp>

namespace petuum {
class PartitionOpLogIndex : boost::noncopyable {
public:
  explicit PartitionOpLogIndex(size_t capacity,
                               int32_t partition_num);
  PartitionOpLogIndex(PartitionOpLogIndex && other);
  PartitionOpLogIndex & operator = (PartitionOpLogIndex && other) = delete;

  ~PartitionOpLogIndex();
  void AddIndex(const std::unordered_set<int32_t> &oplog_index);
  cuckoohash_map<int32_t, bool> *Reset();
  size_t GetNumRowOpLogs();
private:
  void Init(int32_t partition_num);
  int32_t partition_num_;
  size_t capacity_;
  SharedMutex smtx_;
  StripedLock<int32_t> locks_;
  cuckoohash_map<int32_t, bool> *shared_oplog_index_;
};

class TableOpLogIndex : boost::noncopyable{
public:
  explicit TableOpLogIndex(size_t capacity);
  void AddIndex(int32_t partition_num,
                const std::unordered_set<int32_t> &oplog_index);
  cuckoohash_map<int32_t, bool> *ResetPartition(int32_t partition_num);
  size_t GetNumRowOpLogs(int32_t partition_num);
private:
  std::vector<PartitionOpLogIndex> partition_oplog_index_;
};
}
