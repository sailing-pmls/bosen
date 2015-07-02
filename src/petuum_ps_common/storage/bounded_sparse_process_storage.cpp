// Author: Dai Wei (wdai@cs.cmu.edu), Jinliang
// Date: 2014.01.23

#include <utility>
#include <petuum_ps_common/storage/bounded_sparse_process_storage.hpp>
#include <petuum_ps_common/include/constants.hpp>

namespace petuum {

BoundedSparseProcessStorage::BoundedSparseProcessStorage(size_t capacity, size_t lock_pool_size) :
  capacity_(capacity), num_rows_(0),
  storage_map_(capacity * kCuckooExpansionFactor),
  clock_lru_(capacity, lock_pool_size), locks_(lock_pool_size) { }

BoundedSparseProcessStorage::~BoundedSparseProcessStorage() {
  // Iterate through storage_map_ and delete client rows.
  for (auto it = storage_map_.begin(); !it.is_end(); ++it) {
    ClientRow* client_row_ptr = (it->second).first;
    delete client_row_ptr;
  }
}

ClientRow *BoundedSparseProcessStorage::Find(int32_t row_id, RowAccessor* row_accessor) {
  CHECK_NOTNULL(row_accessor);
  std::pair<ClientRow*, int32_t> row_info;
  // Lock to avoid eviction before incrementing ref count of client_row_ptr.
  Unlocker<> unlocker;
  locks_.Lock(row_id, &unlocker);
  bool found = storage_map_.find(row_id, row_info);
  if (found) {
    CHECK_NOTNULL(row_info.first);
    ClientRow* client_row_ptr = row_info.first;
    // SetClientRow() increments the ref count and needs to be protected by
    // lock.
    row_accessor->SetClientRow(client_row_ptr);
    clock_lru_.Reference(row_info.second);
    return client_row_ptr;
  }
  return 0;
}

bool BoundedSparseProcessStorage::Find(int32_t row_id) {
  std::pair<ClientRow*, int32_t> row_info;
  bool found = storage_map_.find(row_id, row_info);
  if (found)
    return true;
  return false;
}

bool BoundedSparseProcessStorage::Insert(int32_t row_id, ClientRow* client_row) {
  // row_id does not exist in storage. Check space and evict if necessary.
  if (capacity_ < (++num_rows_)) {
    std::pair<int32_t, ClientRow*> evicted = EvictOneRow();
    delete evicted.second;
  }
  { // This time we can insert for sure.
    Unlocker<> unlocker;
    locks_.Lock(row_id, &unlocker);
    if (Find(row_id)) {
      return false;
    }
    // Now we can insert row_id without worrying exceeding capacity.
    std::pair<ClientRow*, int32_t> row_info;
    row_info.first = client_row;
    row_info.second = clock_lru_.Insert(row_id);
    CHECK(storage_map_.insert(row_id, row_info));
  }
  return true;
}

// ==================== Private Methods ======================

std::pair<int32_t, ClientRow*> BoundedSparseProcessStorage::EvictOneRow() {
  --num_rows_;
  while (true) {
    int32_t evict_candidate = clock_lru_.FindOneToEvict();
    // Lock to prevent concurrent insert on evict_candidate.
    Unlocker<> unlocker;
    locks_.Lock(evict_candidate, &unlocker);
    std::pair<ClientRow*, int32_t> row_info;
    CHECK(storage_map_.find(evict_candidate, row_info))
        << "row " << evict_candidate << "cannot possibly be evicted while "
        << "the lock on the slot for evict_candidate is held. Report bug.";
    ClientRow* candidate_client_row_ptr = row_info.first;

    if (candidate_client_row_ptr->HasZeroRef()) {
      // erase() and Evict() can be called in either order
      storage_map_.erase(evict_candidate);
      clock_lru_.Evict(row_info.second);
      return std::pair<int32_t, ClientRow*>(evict_candidate,
                                            candidate_client_row_ptr);
    } else {
      // Can't evict with non-zero ref count.
      clock_lru_.NoEvict(row_info.second);
    }
  }
}

}  // namespace petuum
