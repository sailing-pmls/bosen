#ifndef PETUUM_ROW_STORAGE_FACTORY
#define PETUUM_ROW_STORAGE_FACTORY

#include "petuum_ps/storage/row_storage.hpp"

namespace petuum {


// RowStorageFactory cannot be created. It is essentially a namespace but I
// (wdai) opt for class since it's called 'Factory'.
template<typename V>
class RowStorageFactory {
  public:
    // Allocate a storage class and release ownership of it.
    static RowStorage<V>* CreateRowStorage(const StorageConfig& storage_config);
  private:
    RowStorageFactory() { }
};

// ================ RowStorageFactory Implementation ================

// TODO(wdai): We need (# of row_type) x (# of storage_type) conditional
// clauses, which is ugly. Use class registration MACRO instead.
template<typename V>
RowStorage<V>* RowStorageFactory<V>::CreateRowStorage(
    const StorageConfig& storage_config) {
  switch (storage_config.storage_type) {
    case LRU_ROW_STORAGE:
      if (storage_config.row_type == DENSE_STALE_ROW) {


        if (storage_config.lru_params_are_defined) {
          return new LRURowStorage<DenseStalenessRow, V>(
              storage_config.max_size,
              storage_config.active_list_size,
              storage_config.num_row_access_to_active);
        }
        return new LRURowStorage<DenseStalenessRow, V>(storage_config.max_size);
      }
      LOG(FATAL) << "Unsupported row_type: " < storage_config.row_type;

    case THREAD_SAFE_LRU_ROW_STORAGE:
      if (storage_config.row_type == DENSE_STALE_ROW) {
        if (storage_config.lru_params_are_defined) {
          return new ThreadSafeLRURowStorage<DenseStalenessRow, V>(
              storage_config.max_size,
              storage_config.active_list_size,
              storage_config.num_row_access_to_active);
        }
        return new ThreadSafeLRURowStorage<DenseStalenessRow, V>(
            storage_config.max_size);
      }
      LOG(FATAL) << "Unsupported row_type: " < storage_config.row_type;

    case CONCURRENT_ROW_STORAGE:
      if (storage_config.row_type == DENSE_STALE_ROW) {
        
        return new ConcurrentLRURowStorage<DenseStalenessRow, V>(
            storage_config.max_size);
      }
      LOG(FATAL) << "Unsupported row_type: " < storage_config.row_type;

    default:
      LOG(FATAL) << "Unrecognized storage_type: "
        << storage_config.storage_type;
  }
}


}  // namespace petuum

#endif  // PETUUM_ROW_STORAGE_FACTORY

