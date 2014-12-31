#ifndef PETUUM_ROW_STORAGE
#define PETUUM_ROW_STORAGE

#include "petuum_ps/include/configs.hpp"

namespace petuum {

// RowStorage is an abstract class representing a basic row storage without
// freshness awareness. This class is extended to support LRU and LFU row
// eviction policy when storage is full, and serves as the common interface
// for ThreadCache, ProcessorCache, and ServerStorage.  Template ROW<V> can be
// sparse or dense representation (e.g., map vs vector), and should implement
// Row<V> "concept".
//
// Comment(wdai): We discourage using HasRow(). Instead, use Get() directly to
// save one map lookup.
//
//
// Comment(wdai): This interface does not support Inc(), which should be
// supported at a higher level of abstraction.
template<template<typename> class ROW, typename V>
class RowStorage {
  public:
    // Default ctor so RowStorage can be initialized in containers.
    RowStorage();

    explicit RowStorage(const StorageConfig& storage_config);

    // Constructor specifying the maximum number of rows to store.
    // Comment (wdai): This disallows default constructor.
    explicit RowStorage(int capacity);

    RowStorage(const RowStorage& other);

    // Get the size of storage.
    int get_size() const;
    void set_size(int new_size);

    // Get the number of rows currently in storage.
    virtual int get_num_rows() const = 0;

    // Replace the entry if row_id already exist (return >0); otherwise
    // it's a no-op (return 0). Return negatives for error code.
    virtual int Put(int32_t row_id, int32_t col_id, const V& val) = 0;

    // Insert (if row_id does not already exist) or replace an existing row..
    // Return 1 when a new row is inserted, 0 when an existing row is
    // replaced, and negatives for error.
    virtual int PutRow(int32_t row_id, const ROW<V>& row_vals) = 0;

    // Check if the storage has a particular row.
    // Comment(wdai): This does not count as an access to a row.
    virtual bool HasRow(int32_t row_id) const = 0;

    // Retrieve an entry from the storage. Return >0 and val if available,
    // otherwise return 0; return negatives for errors (e.g. col_id out of
    // bound).
    //
    // Comment(wdai): This is not a const function since Get() counts as one
    // access and will effect underlying data structure in LRU/LFU policy.
    //
    // TODO(wdai): Need to handle out-of-bound Get() and return negative error
    // codes.
    virtual int Get(int32_t row_id, int32_t col_id, V* val) = 0;

    // Same as the other Get(), but retrieve row_iter as well.
    virtual int Get(int32_t row_id, int32_t col_id, V* val,
        int32_t *row_iter) = 0;

    // Retrieve the entire row with row iteration >= row_iter. Return >0 and
    // ROW<V> if available; return 0 if not; negative for errors.
    virtual int GetRow(int32_t row_id, ROW<V>* row_data,
        int32_t row_iter = 0) = 0;

    // Atomically increment an entry by delta using += operator of V. Return
    // >0 if the row is in the storage, 0 if not found (and thus a no-op). <0
    // for errors.
    virtual int Inc(int32_t row_id, int32_t col_id, const V& delta) = 0;

    // Erase a row if it's in storage. No-op otherwise.
    virtual void Erase(int32_t row_id) = 0;

  protected:
    // Max number of rows to store, immutable after instantiation.
    int capacity_;
};

// =============== RowStorage Implementation ===============

template<template<typename> class ROW, typename V>
RowStorage<ROW, V>::RowStorage()
  : capacity_(0) { }

template<template<typename> class ROW, typename V>
RowStorage<ROW, V>::RowStorage(int capacity)
  : capacity_(capacity) { }

template<template<typename> class ROW, typename V>
RowStorage<ROW, V>::RowStorage(const StorageConfig& storage_config)
  : capacity_(storage_config.capacity) { }

template<template<typename> class ROW, typename V>
RowStorage<ROW, V>::RowStorage(const RowStorage& other)
  : capacity_(other.capacity_) { }

template<template<typename> class ROW, typename V>
int RowStorage<ROW, V>::get_size() const {
  return capacity_;
}

template<template<typename> class ROW, typename V>
void RowStorage<ROW, V>::set_size(int new_size) {
  capacity_ = new_size;
}

}  // namespace petuum

#endif  // PETUUM_ROW_STORAGE
