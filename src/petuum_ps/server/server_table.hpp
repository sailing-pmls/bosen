// author: jinliang

#pragma once
#include "petuum_ps/server/server_row.hpp"
#include "petuum_ps/util/class_register.hpp"
#include <boost/unordered_map.hpp>
#include <map>
#include <utility>

namespace petuum {

class ServerTable : boost::noncopyable {
public:
  explicit ServerTable(const TableInfo &table_info):
      table_info_(table_info),
      tmp_row_buff_size_ (kTmpRowBuffSizeInit) {}

  // Move constructor: storage gets other's storage, leaving other
  // in an unspecified but valid state.
  ServerTable(ServerTable && other):
    table_info_(other.table_info_),
    storage_(std::move(other.storage_)) ,
    tmp_row_buff_size_(other.tmp_row_buff_size_) { }

  ServerRow *FindRow(int32_t row_id) {
    auto row_iter = storage_.find(row_id);
    if(row_iter == storage_.end())
      return 0;
    return &(row_iter->second);
  }

  ServerRow *CreateRow(int32_t row_id) {
    int32_t row_type = table_info_.row_type;
    AbstractRow *row_data
      = ClassRegistry<AbstractRow>::GetRegistry().CreateObject(row_type);
    row_data->Init(table_info_.row_capacity);
    storage_.insert(std::make_pair(row_id, ServerRow(row_data)));
    return &(storage_[row_id]);
  }

  bool ApplyRowOpLog(int32_t row_id, const int32_t *column_ids,
    const void *updates, int32_t num_updates){
    auto row_iter = storage_.find(row_id);
    if (row_iter == storage_.end()) {
      //VLOG(0) << "Row " << row_id << " is not found!";
      return false;
    }
    row_iter->second.ApplyBatchInc(column_ids, updates, num_updates);
    return true;
  }

  void InitAppendTableToBuffs() {
    row_iter_ = storage_.begin();
    VLOG(0) << "tmp_row_buff_size_ = " << tmp_row_buff_size_;
    tmp_row_buff_ = new uint8_t[tmp_row_buff_size_];
  }

  bool AppendTableToBuffs(int32_t client_id_st,
    boost::unordered_map<int32_t, RecordBuff> *buffs, int32_t *failed_bg_id,
    int32_t *failed_client_id, bool resume) {

    if (resume) {
      bool append_row_suc = row_iter_->second.AppendRowToBuffs(client_id_st,
        buffs, tmp_row_buff_, curr_row_size_, row_iter_->first,failed_bg_id,
        failed_client_id);
      if (!append_row_suc)
        return false;
      ++row_iter_;
    }
    for (; row_iter_ != storage_.end(); ++row_iter_) {
      if (row_iter_->second.NoClientSubscribed())
        continue;
      //VLOG(0) << "Appending row " << row_iter_->first;
      curr_row_size_ = row_iter_->second.SerializedSize();
      //VLOG(0) << "Get serialized size = " << curr_row_size_;
      if (curr_row_size_ > tmp_row_buff_size_) {
        VLOG(0) << "Reallocate tmp_buff "
                << " curr_row_size_ = " << curr_row_size_
                << " tmp_row_buff_size_ = " << tmp_row_buff_size_;
        delete[] tmp_row_buff_;
        tmp_row_buff_size_ = curr_row_size_;
        tmp_row_buff_ = new uint8_t[curr_row_size_];
      }
      curr_row_size_ = row_iter_->second.Serialize(tmp_row_buff_);
      //VLOG(0) << "Serialize Done!, curr_row_size_ = " << curr_row_size_;
      bool append_row_suc = row_iter_->second.AppendRowToBuffs(client_id_st,
        buffs, tmp_row_buff_, curr_row_size_, row_iter_->first, failed_bg_id,
        failed_client_id);
      if (!append_row_suc)
        return false;
    }
    //VLOG(0) << "Appending table done";
    delete[] tmp_row_buff_;
    return true;
  }

private:
  TableInfo table_info_;
  boost::unordered_map<int32_t, ServerRow> storage_;

  // used for appending rows to buffs
  boost::unordered_map<int32_t, ServerRow>::iterator row_iter_;
  uint8_t *tmp_row_buff_;
  size_t tmp_row_buff_size_;
  static const size_t kTmpRowBuffSizeInit = 512;
  size_t curr_row_size_;
};

}
