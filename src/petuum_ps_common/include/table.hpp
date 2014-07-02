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
// table.hpp
// author: jinliang

#pragma once

#include <boost/utility.hpp>
#include <vector>

#include <petuum_ps_common/include/row_access.hpp>
#include <petuum_ps_common/client/abstract_client_table.hpp>

namespace petuum {

// UPDATE can be V = {int, double, ...} when the entry is simple numerical
// type. If the entry type is a struct, UPDATE can be id:val pair, like {1:5}
// which increment field 1 of struct by 5.
template<typename UPDATE>
class UpdateBatch {
public:
  UpdateBatch():
    col_ids_(0),
    updates_(0) { }

  UpdateBatch(size_t num_updates):
      col_ids_(num_updates),
      updates_(num_updates) { }

  ~UpdateBatch() { }

  UpdateBatch (const UpdateBatch & other) :
      col_ids_(other.col_ids_),
      updates_(other.updates_) { }

  UpdateBatch & operator = (const UpdateBatch &other) {
    col_ids_ = other.col_ids_;
    updates_ = other.updates_;
    return *this;
  }

  void Update(int32_t column_id, const UPDATE& update) {
    col_ids_.push_back(column_id);
    updates_.push_back(update);
  }

  void UpdateSet(int32_t idx, int32_t column_id, const UPDATE& update) {
    col_ids_[idx] = column_id;
    updates_[idx] = update;
  }

  const std::vector<int32_t> &GetColIDs() const {
    return col_ids_;
  }

  const UPDATE* GetUpdates() const {
    return updates_.data();
  }

  int32_t GetBatchSize() const {
    return updates_.size();
  }

private:
  std::vector<int32_t> col_ids_;
  std::vector<UPDATE> updates_;
};

// User table is stores a lightweight pointer to ClientTable.
template<typename UPDATE>
class Table {
public:
  Table():
    system_table_(0){}
  Table(AbstractClientTable* system_table):
    system_table_(system_table){}

  Table(const Table &table):
    system_table_(table.system_table_){}

  Table & operator = (const Table &table){
    system_table_ = table.system_table_;
    return *this;
  }

  void GetAsync(int32_t row_id){
    system_table_->GetAsync(row_id);
  }

  void WaitPendingAsyncGet() {
    system_table_->WaitPendingAsyncGet();
  }

  void ThreadGet(int32_t row_id, ThreadRowAccessor* row_accessor){
    system_table_->ThreadGet(row_id, row_accessor);
  }

  void ThreadInc(int32_t row_id, int32_t column_id, UPDATE update){
    system_table_->ThreadInc(row_id, column_id, &update);
  }

  void ThreadBatchInc(int32_t row_id, const UpdateBatch<UPDATE>& update_batch){
    system_table_->ThreadBatchInc(row_id, update_batch.GetColIDs().data(),
      update_batch.GetUpdates(), update_batch.GetBatchSize());
  }

  void FlushThreadCache() {
    system_table_->FlushThreadCache();
  }

  // row_accessor helps maintain the reference count to prevent premature
  // cache eviction. The lock
  void Get(int32_t row_id, RowAccessor* row_accessor){
    system_table_->Get(row_id, row_accessor);
  }

  void Inc(int32_t row_id, int32_t column_id, UPDATE update){
    system_table_->Inc(row_id, column_id, &update);
  }

  void BatchInc(int32_t row_id, const UpdateBatch<UPDATE>& update_batch){
    system_table_->BatchInc(row_id, update_batch.GetColIDs().data(),
      update_batch.GetUpdates(), update_batch.GetBatchSize());
  }

  int32_t get_row_type() const {
    return system_table_->get_row_type();
  }

private:
  AbstractClientTable* system_table_;
};
}   // namespace petuum
