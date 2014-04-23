
#include "petuum_ps/oplog/oplog_index.hpp"

namespace petuum {

PartitionOpLogIndex::PartitionOpLogIndex(size_t capacity):
    capacity_(capacity),
    locks_(GlobalContext::get_lock_pool_size()),
    shared_oplog_index_(new cuckoohash_map<int32_t, bool>
                    (capacity*GlobalContext::get_cuckoo_expansion_factor())){ }

PartitionOpLogIndex::~PartitionOpLogIndex() {
  delete shared_oplog_index_;
}

PartitionOpLogIndex::PartitionOpLogIndex(PartitionOpLogIndex && other) {
  cuckoohash_map<int32_t, bool> *tmp_index_ptr = shared_oplog_index_;
  size_t tmp_capacity = capacity_;
  shared_oplog_index_ = other.shared_oplog_index_;
  other.shared_oplog_index_ = tmp_index_ptr;
  capacity_ = other.capacity_;
  other.capacity_ = tmp_capacity;
}

void PartitionOpLogIndex::AddIndex(const boost::unordered_map<int32_t, bool>
                                   &oplog_index) {
  smtx_.lock_shared();
  for (auto iter = oplog_index.cbegin(); iter != oplog_index.cend(); iter++) {
    locks_.Lock(iter->first);
    shared_oplog_index_->insert(iter->first, true);
    locks_.Unlock(iter->first);
  }
  smtx_.unlock_shared();
}

cuckoohash_map<int32_t, bool> *PartitionOpLogIndex::Reset() {
  smtx_.lock();
  cuckoohash_map<int32_t, bool> *old_index = shared_oplog_index_;
  shared_oplog_index_ = new cuckoohash_map<int32_t, bool>
                    (capacity_*GlobalContext::get_cuckoo_expansion_factor());
  smtx_.unlock();
  return old_index;
}

TableOpLogIndex::TableOpLogIndex(size_t capacity) {
  for (int32_t i = 0; i < GlobalContext::get_num_bg_threads(); ++i) {
    partition_oplog_index_.emplace_back(capacity);
  }
}

void TableOpLogIndex::AddIndex(int32_t partition_num,
                               const boost::unordered_map<int32_t, bool>
                               &oplog_index) {
  partition_oplog_index_[partition_num].AddIndex(oplog_index);
}

cuckoohash_map<int32_t, bool> *TableOpLogIndex::ResetPartition(
    int32_t partition_num) {
  return partition_oplog_index_[partition_num].Reset();
}

}
