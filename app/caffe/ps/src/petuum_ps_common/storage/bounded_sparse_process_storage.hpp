// Author: Dai Wei (wdai@cs.cmu.edu), jinliang

#pragma once

#include <petuum_ps_common/include/row_access.hpp>
#include <petuum_ps_common/client/client_row.hpp>
#include <petuum_ps_common/util/striped_lock.hpp>
#include <petuum_ps_common/storage/clock_lru.hpp>
#include <petuum_ps_common/storage/abstract_process_storage.hpp>
#include <libcuckoo/cuckoohash_map.hh>
#include <atomic>
#include <utility>
#include <cstdint>

namespace petuum {

// A ProcessStorage type whose size is upper bounded by its capacity.
// During insertion, the actual size of the process storage may exceed
// capacity temoprarily, up to capacity + N, where N is the number
// of threads that are concurrently invoking insert.

// LRU-based eviction is performed to ensure the size of the storage to
// not exceed the pre-specified capacity. Eviction can only happen to
// rows that no RowAccessor refers to.

class BoundedSparseProcessStorage : public AbstractProcessStorage {
public:
  // capacity is the upper bound of the number of rows this ProcessStorage
  // can store.
  BoundedSparseProcessStorage(size_t capacity, size_t lock_pool_size);

  ~BoundedSparseProcessStorage();

  // Find row row_id. Return true if found, otherwise false.
  ClientRow *Find(int32_t row_id, RowAccessor* row_accessor);

  // Check if a row exists, does not count as one access
  bool Find(int32_t row_id);

  // The following cases may cause eviction to occur when it
  // is unnecessary:
  // 1) when inserting a row that has already existed in the process storage;
  // 2) when multiple threads perform insertion concurrently.
  // To ensure correct behavior of insertion, it is needed to ensure that
  // the capacity is larger than or equal to the number of concurrent insertion
  // threads, otherwise the insertion thread may get stuck in an infinite loop
  // during eviction.
  // Since eviction may only happen to rows that are not being referenced, therefore
  // we require that capacity - number of referenced rows to be larger than or equal
  // to the number of concurrent insertion threads.
  bool Insert(int32_t row_id, ClientRow* client_row);

private:
  // Evict one inactive row using CLOCK replacement algorithm.
  std::pair<int32_t, ClientRow*> EvictOneRow();

  // Number of rows allowed in this storage.
  size_t capacity_;

  std::atomic<size_t> num_rows_;

  // The key type is row_id (int32_t), and the value type consists of
  // a ClientRow* pointer and a slot index (int32_t).
  cuckoohash_map<int32_t, std::pair<ClientRow*, int32_t> > storage_map_;

  ClockLRU clock_lru_;

  // Lock pool.
  StripedLock<int32_t> locks_;
};


}  // namespace petuum
