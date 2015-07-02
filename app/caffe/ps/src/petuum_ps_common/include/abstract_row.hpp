#pragma once

#include <boost/thread.hpp>
#include <vector>
#include <boost/shared_array.hpp>
#include <boost/noncopyable.hpp>

namespace petuum {

// This class defines the interface of the Row type.  ApplyUpdate() and
// ApplyBatchUpdate() have to be concurrent with each other and with other
// functions that may be invoked by application threads.  Petuum system does
// not require thread safety of other functions.
class AbstractRow : boost::noncopyable {
public:
  virtual ~AbstractRow() { }

  virtual void Init(size_t capacity) = 0;

  virtual AbstractRow *Clone() const = 0;

  virtual size_t get_update_size() const = 0;

  // Upper bound of the number of bytes that serialized row shall occupy.
  // Find some balance between tightness and time complexity.
  virtual size_t SerializedSize() const = 0;

  // Bytes points to a chunk of allocated memory whose size is guaranteed to
  // be at least SerializedSize(). Need not be thread safe. Return the exact
  // size of serialized row.
  virtual size_t Serialize(void *bytes) const = 0;

  // Deserialize and initialize a row. Init is not called yet.
  // Return true on success, false otherwise. Need not be thread-safe.
  virtual void Deserialize(const void* data, size_t num_bytes) = 0;

  // Need not be thread-safe.
  // Init or Deserialize has been called on this row.
  virtual void ResetRowData(const void *data, size_t num_bytes) = 0;

  // Write lock ensure mutual exclusiveness for non-thread-safe functions
  virtual void GetWriteLock() const = 0;
  virtual void ReleaseWriteLock() const = 0;

  // Thread safe.
  virtual double ApplyIncGetImportance(int32_t column_id, const void *update) = 0;

  // Thread safe. Return importance
  // Updates are stored contiguously in the memory pointed by update_batch.
  virtual double ApplyBatchIncGetImportance(const int32_t *column_ids,
    const void* update_batch, int32_t num_updates) = 0;

  // Not necessarily thread-safe.
  // PS guarantees to not call this function concurrently with other functions
  // or itself.
  virtual double ApplyIncUnsafeGetImportance(int32_t column_id,
                                             const void *update) = 0;

  virtual double ApplyBatchIncUnsafeGetImportance(
      const int32_t *column_ids,
      const void* update_batch, int32_t num_updates) = 0;

  // Thread safe.
  virtual void ApplyInc(int32_t column_id, const void *update) = 0;

  // Thread safe.
  // Updates are stored contiguously in the memory pointed by update_batch.
  virtual void ApplyBatchInc(const int32_t *column_ids,
    const void* update_batch, int32_t num_updates) = 0;

  // Not necessarily thread-safe.
  // PS guarantees to not call this function concurrently with other functions
  // or itself.
  virtual void ApplyIncUnsafe(int32_t column_id, const void *update) = 0;

  virtual void ApplyBatchIncUnsafe(const int32_t *column_ids,
    const void* update_batch, int32_t num_updates) = 0;

  // The update batch contains an update for each each element within the
  // capacity of the row, in the order of increasing column_ids.
  virtual double ApplyDenseBatchIncGetImportance(
      const void* update_batch, int32_t index_st, int32_t num_updates) = 0;

  virtual void ApplyDenseBatchInc(
      const void* update_batch, int32_t index_st, int32_t num_updates) = 0;

  virtual double ApplyDenseBatchIncUnsafeGetImportance(
      const void* update_batch, int32_t index_st, int32_t num_updates) = 0;

  virtual void ApplyDenseBatchIncUnsafe(
      const void* update_batch, int32_t index_st, int32_t num_updates) = 0;

  // Aggregate update1 and update2 by summation and substraction (update1 -
  // update2), outputing to update2. column_id is optionally used in case
  // updates are applied differently for different column of a row.
  //
  // Both AddUpdates and SubstractUpdates should behave like a static
  // method.  But we cannot have virtual static method.
  // Need be thread-safe and better be concurrent.
  // Those functions must work correctly without Init() or Deserialize()
  virtual void AddUpdates(int32_t column_id, void* update1,
                          const void* update2) const = 0;

  virtual void SubtractUpdates(int32_t column_id, void *update1,
                               const void* update2) const = 0;

  // Get importance of this update as if it is applied on to the given value.
  virtual double GetImportance(int32_t column_id, const void *update,
                               const void *value) const = 0;

  virtual double GetImportance(int32_t column_id, const void *update) const = 0;

  virtual double GetAccumImportance(
      const int32_t *column_ids, const void *update_batch,
      int32_t num_updates) const = 0;

  virtual double GetDenseAccumImportance(
      const void *update_batch, int32_t index_st,
      int32_t num_updates) const = 0;

  // Initialize update. Initialized update represents "zero update".
  // In other words, 0 + u = u (0 is the zero update).
  virtual void InitUpdate(int32_t column_id, void* zero) const = 0;

  virtual bool CheckZeroUpdate(const void *update) const = 0;
};

}   // namespace petuum
