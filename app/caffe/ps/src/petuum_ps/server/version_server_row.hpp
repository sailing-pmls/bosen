// author: jinliang

#include <petuum_ps/server/server_row.hpp>

#pragma once

namespace petuum {

// Disallow copy to avoid shared ownership of row_data.
// Allow move sematic for it to be stored in STL containers.
class VersionServerRow : public ServerRow {
public:
  VersionServerRow():
      ServerRow(),
      version_(1) { }

  explicit VersionServerRow(AbstractRow *row_data):
      ServerRow(row_data),
      version_(1) { }


  VersionServerRow(VersionServerRow && other):
      ServerRow(std::move(other)),
      version_(other.version_) { }

  VersionServerRow & operator = (VersionServerRow & other) = delete;

  void ApplyBatchInc(
      const int32_t *column_ids,
      const void *update_batch, int32_t num_updates) {
    ServerRow::ApplyBatchInc(column_ids, update_batch, num_updates);
    version_++;
  }

  void ApplyBatchIncAccumImportance(
      const int32_t *column_ids,
      const void *update_batch, int32_t num_updates) {
    ServerRow::ApplyBatchIncAccumImportance(column_ids,
                                            update_batch,
                                            num_updates);
    version_++;
  }

  void ApplyDenseBatchInc(const void *update_batch, int32_t num_updates) {
    ServerRow::ApplyDenseBatchInc(update_batch, num_updates);
    version_++;
  }

  void ApplyDenseBatchIncAccumImportance(const void *update_batch,
                                         int32_t num_updates) {
    ServerRow::ApplyDenseBatchIncAccumImportance(update_batch, num_updates);
    version_++;
  }

  size_t SerializedSize() const {
    return row_data_->SerializedSize() + sizeof(uint64_t);
  }

  size_t Serialize(void *bytes) const {
    size_t offset = row_data_->Serialize(bytes);
    uint8_t *bytes_uint8 = reinterpret_cast<uint8_t*>(bytes);
    *(reinterpret_cast<uint64_t*>(bytes_uint8 + offset)) = version_;
    return offset + sizeof(uint64_t);
  }

  virtual uint64_t get_version() { return version_; }

private:
  uint64_t version_;
};
}
