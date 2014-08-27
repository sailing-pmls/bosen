// Copyright (c) 2014, Sailing Lab
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the <ORGANIZATION> nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
// author: jinliang

#include <petuum_ps_common/include/abstract_row.hpp>
#include <petuum_ps/server/callback_subs.hpp>

#include <boost/noncopyable.hpp>

#pragma once

namespace petuum {

// Disallow copy to avoid shared ownership of row_data.
// Allow move sematic for it to be stored in STL containers.
class ServerRow : boost::noncopyable {
public:
  ServerRow():
    dirty_(false) { }
  ServerRow(AbstractRow *row_data):
      row_data_(row_data),
      num_clients_subscribed_(0),
      dirty_(false) { }

  ~ServerRow() {
    if(row_data_ != 0)
      delete row_data_;
  }

  ServerRow(ServerRow && other):
      row_data_(other.row_data_),
      num_clients_subscribed_(other.num_clients_subscribed_),
      dirty_(other.dirty_) {
    other.row_data_ = 0;
  }

  ServerRow & operator = (ServerRow & other) = delete;

  void ApplyBatchInc(const int32_t *column_ids,
    const void *update_batch, int32_t num_updates) {
    row_data_->ApplyBatchIncUnsafe(column_ids, update_batch, num_updates);

    dirty_ = true;
  }

  size_t SerializedSize() const {
    return row_data_->SerializedSize();
  }

  size_t Serialize(void *bytes) const {
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

  bool IsDirty() {
    return dirty_;
  }

  void ResetDirty() {
    dirty_ = false;
  }

private:
  CallBackSubs callback_subs_;
  AbstractRow *row_data_;
  size_t num_clients_subscribed_;

  bool dirty_;
};
}
