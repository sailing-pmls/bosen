// author: jinliang

#include "petuum_ps/include/abstract_row.hpp"
#include "petuum_ps/server/callback_subs.hpp"

#include <boost/noncopyable.hpp>

#pragma once

namespace petuum {

// Disallow copy to avoid shared ownership of row_data.
// Allow move sematic for it to be stored in STL containers.
class ServerRow : boost::noncopyable {
public:
  ServerRow() {}
  ServerRow(AbstractRow *row_data):
      row_data_(row_data),
      num_clients_subscribed_(0) { }

  ~ServerRow() {
    if(row_data_ != 0)
      delete row_data_;
  }

  ServerRow(ServerRow && other):
      row_data_(other.row_data_),
      num_clients_subscribed_(other.num_clients_subscribed_) {
    other.row_data_ = 0;
  }

  void ApplyBatchInc(const int32_t *column_ids,
    const void *update_batch, int32_t num_updates) {
    row_data_->ApplyBatchIncUnsafe(column_ids, update_batch, num_updates);
  }

  size_t SerializedSize() {
    //VLOG(0) << "SerializedSize(), row_data_ = " << row_data_;
    return row_data_->SerializedSize();
  }

  size_t Serialize(void *bytes) {
    return row_data_->Serialize(bytes);
  }

  void Subscribe(int32_t client_id) {
    if (callback_subs_.Subscribe(client_id))
      ++num_clients_subscribed_;
  }

  bool NoClientSubscribed() {
    //VLOG(0) << "NoClientSubscribed num_clients_subscribed_ = "
    //      << num_clients_subscribed_;
    return (num_clients_subscribed_ == 0);
  }

  void Unsubscribe(int32_t client_id) {
    if (callback_subs_.Unsubscribe(client_id))
      --num_clients_subscribed_;
  }

  bool AppendRowToBuffs(int32_t client_id_st,
    boost::unordered_map<int32_t, RecordBuff> *buffs,
    const void *row_data, size_t row_size, int32_t row_id,
    int32_t *failed_bg_id, int32_t *failed_client_id) {
    return callback_subs_.AppendRowToBuffs(client_id_st, buffs, row_data,
      row_size, row_id, failed_bg_id, failed_client_id);
  }

private:
  CallBackSubs callback_subs_;
  AbstractRow *row_data_;
  size_t num_clients_subscribed_;
};
}
