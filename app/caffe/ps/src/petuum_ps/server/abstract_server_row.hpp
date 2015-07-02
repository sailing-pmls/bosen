// author: jinliang

#include <petuum_ps_common/include/abstract_row.hpp>
#include <petuum_ps/server/callback_subs.hpp>
#include <boost/noncopyable.hpp>

#pragma once

namespace petuum {

// Disallow copy to avoid shared ownership of row_data.
// Allow move sematic for it to be stored in STL containers.
class AbstractServerRow : boost::noncopyable {
public:
  AbstractServerRow() { }

  virtual ~AbstractServerRow() { }

  AbstractServerRow(AbstractServerRow && other) { }

  AbstractServerRow & operator = (AbstractServerRow & other) = delete;

  virtual void ApplyBatchInc(
      const int32_t *column_ids,
      const void *update_batch, int32_t num_updates) = 0;

  virtual void ApplyBatchIncAccumImportance(
      const int32_t *column_ids,
      const void *update_batch, int32_t num_updates) = 0;

  virtual void ApplyDenseBatchInc(const void *update_batch,
                                  int32_t num_updates) = 0;

  virtual void ApplyDenseBatchIncAccumImportance(const void *update_batch,
                                         int32_t num_updates) = 0;

  virtual size_t SerializedSize() const = 0;

  virtual size_t Serialize(void *bytes) const = 0;

  virtual void Subscribe(int32_t client_id) = 0;

  virtual bool NoClientSubscribed() const = 0;

  virtual void Unsubscribe(int32_t client_id) = 0;

  virtual bool AppendRowToBuffs(
      int32_t client_id_st,
      boost::unordered_map<int32_t, RecordBuff> *buffs,
      const void *row_data, size_t row_size, int32_t row_id,
      int32_t *failed_client_id, uint64_t *num_clients) = 0;

  virtual bool IsDirty() const = 0;

  virtual void ResetDirty() = 0;

  virtual void AccumSerializedSizePerClient(
      boost::unordered_map<int32_t, size_t> *client_size_map) = 0;

  virtual void AppendRowToBuffs(
      boost::unordered_map<int32_t, RecordBuff> *buffs,
      const void *row_data, size_t row_size, int32_t row_id,
      size_t *num_clients) = 0;

  virtual double get_importance() = 0;

  virtual void AccumImportance(double importance) = 0;

  virtual void ResetImportance() = 0;

  virtual uint64_t get_version() { return 0; }
};
}
