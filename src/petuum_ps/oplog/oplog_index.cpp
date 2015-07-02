
#include <petuum_ps/oplog/oplog_index.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps_common/include/constants.hpp>
#include <glog/logging.h>

namespace petuum {

PartitionOpLogIndex::PartitionOpLogIndex(size_t capacity):
    capacity_(capacity),
    locks_(GlobalContext::GetLockPoolSize()),
    shared_oplog_index_(new cuckoohash_map<int32_t, bool>
                        (capacity*kCuckooExpansionFactor)){
}

PartitionOpLogIndex::~PartitionOpLogIndex() {
  if (shared_oplog_index_ != 0)
    delete shared_oplog_index_;
}


PartitionOpLogIndex::PartitionOpLogIndex(PartitionOpLogIndex && other):
  capacity_(other.capacity_),
  shared_oplog_index_(other.shared_oplog_index_) {
  other.shared_oplog_index_ = 0;
}

void PartitionOpLogIndex::AddIndex(const std::unordered_set<int32_t>
                                   &oplog_index) {
  smtx_.lock_shared();
  for (auto iter = oplog_index.cbegin(); iter != oplog_index.cend(); iter++) {
    locks_.Lock(*iter);
    shared_oplog_index_->insert(*iter, true);
    locks_.Unlock(*iter);
  }
  smtx_.unlock_shared();
}

cuckoohash_map<int32_t, bool> *PartitionOpLogIndex::Reset() {
  smtx_.lock();
  cuckoohash_map<int32_t, bool> *old_index = shared_oplog_index_;
  shared_oplog_index_ = new cuckoohash_map<int32_t, bool>
                    (capacity_*kCuckooExpansionFactor);
  smtx_.unlock();
  return old_index;
}

size_t PartitionOpLogIndex::GetNumRowOpLogs() {
  smtx_.lock();
  size_t num_row_oplogs = shared_oplog_index_->size();
  smtx_.unlock();
  return num_row_oplogs;
}

TableOpLogIndex::TableOpLogIndex(size_t capacity) {
  for (int32_t i = 0; i < GlobalContext::get_num_comm_channels_per_client();
       ++i) {
    partition_oplog_index_.emplace_back(capacity);
  }
}

void TableOpLogIndex::AddIndex(int32_t partition_num,
                               const std::unordered_set<int32_t>
                               &oplog_index) {

  partition_oplog_index_[partition_num].AddIndex(oplog_index);
}

cuckoohash_map<int32_t, bool> *TableOpLogIndex::ResetPartition(
    int32_t partition_num) {
  return partition_oplog_index_[partition_num].Reset();
}

size_t TableOpLogIndex::GetNumRowOpLogs(int32_t partition_num) {
  return partition_oplog_index_[partition_num].GetNumRowOpLogs();
}

}
