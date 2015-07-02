#pragma once

#include <memory>
#include <vector>
#include <boost/noncopyable.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps_common/include/constants.hpp>
#include <petuum_ps_common/oplog/abstract_row_oplog.hpp>
#include <glog/logging.h>

namespace petuum {

struct SerializedOpLogBuffer : boost::noncopyable {
public:
  typedef size_t (*GetSerializedRowOpLogSizeFunc)(AbstractRowOpLog *row_oplog);

  static size_t GetDenseSerializedRowOpLogSize(AbstractRowOpLog *row_oplog) {
    return row_oplog->GetDenseSerializedSize();
  }

  static size_t GetSparseSerializedRowOpLogSize(AbstractRowOpLog *row_oplog) {
    row_oplog->ClearZerosAndGetNoneZeroSize();
    return row_oplog->GetSparseSerializedSize();
  }

  SerializedOpLogBuffer(bool dense_serialize):
      size_(0),
      mem_(new uint8_t[capacity_]),
      num_row_oplogs_(0) {
    if (dense_serialize) {
      SerializeOpLog_ = &AbstractRowOpLog::SerializeDense;
      GetSerializedRowOpLogSize_ = GetDenseSerializedRowOpLogSize;
    } else {
      SerializeOpLog_ = &AbstractRowOpLog::SerializeSparse;
      GetSerializedRowOpLogSize_ = GetSparseSerializedRowOpLogSize;
    }
  }

  ~SerializedOpLogBuffer() {
    delete[] mem_;
  }

  size_t AppendRowOpLog(int32_t row_id, AbstractRowOpLog *row_oplog) {
    size_t serialized_size = GetSerializedRowOpLogSize_(row_oplog);
    if (size_ + sizeof(int32_t) + serialized_size > capacity_)
      return 0;

    *(reinterpret_cast<int32_t*>(mem_ + size_)) = row_id;
    size_ += sizeof(int32_t);

    serialized_size = (row_oplog->*SerializeOpLog_)(mem_ + size_);
    size_ += serialized_size;
    ++num_row_oplogs_;

    return sizeof(int32_t) + serialized_size;
  }

  size_t get_size() const {
    return size_;
  }

  const uint8_t *get_mem() const {
    return mem_;
  }

  size_t get_num_row_oplogs() const {
    return num_row_oplogs_;
  }

private:
  static const size_t capacity_ = 4*k1_Mi;
  size_t size_;
  uint8_t* mem_;
  AbstractRowOpLog::SerializeFunc SerializeOpLog_;
  GetSerializedRowOpLogSizeFunc GetSerializedRowOpLogSize_;
  size_t num_row_oplogs_;
};

class RowOpLogSerializer : boost::noncopyable {
public:
  RowOpLogSerializer(bool dense_serialize,
                     int32_t my_comm_channel_idx):
      dense_serialize_(dense_serialize),
      my_comm_channel_idx_(my_comm_channel_idx),
      total_accum_size_(0),
      total_accum_num_rows_(0) { }

  ~RowOpLogSerializer() {
    // CHECK_EQ(buffer_map_.size(), 0);
  }

  size_t get_total_accum_size() const {
    return total_accum_size_;
  }

  size_t get_total_accum_num_rows() const {
    return total_accum_num_rows_;
  }

  size_t AppendRowOpLog(int32_t row_id, AbstractRowOpLog *row_oplog) {
    int32_t server_id = GlobalContext::GetPartitionServerID(
        row_id, my_comm_channel_idx_);

    auto map_iter = buffer_map_.find(server_id);

    if (map_iter == buffer_map_.end()) {
      buffer_map_.insert(std::make_pair(
          server_id, std::vector<SerializedOpLogBuffer*>(1) ) );
      map_iter = buffer_map_.find(server_id);
      (map_iter->second)[0] = new SerializedOpLogBuffer(dense_serialize_);
    }

    SerializedOpLogBuffer *buffer = map_iter->second.back();
    size_t serialized_size = buffer->AppendRowOpLog(row_id, row_oplog);
    if (serialized_size == 0) {
      SerializedOpLogBuffer *new_buffer = new SerializedOpLogBuffer(dense_serialize_);
      serialized_size = new_buffer->AppendRowOpLog(row_id, row_oplog);
      CHECK_GT(serialized_size, 0) << "row id = " << row_id;
      map_iter->second.push_back(new_buffer);
    }
    total_accum_size_ += serialized_size;
    total_accum_num_rows_++;
    return serialized_size;
  }

  // assme map entries are already reset to 0
  void GetServerTableSizeMap(std::map<int32_t, size_t> *table_num_bytes_by_server) {
    for (auto &buff_pair : buffer_map_) {
      std::vector<SerializedOpLogBuffer*> &buff_vec = buff_pair.second;
      CHECK(buff_vec.size() > 0);
      size_t &per_server_table_size = (*table_num_bytes_by_server)[buff_pair.first];
      per_server_table_size = 0;
      for (auto &buff : buff_vec) {
        per_server_table_size += buff->get_size();
      }
    }
  }

  void SerializeByServer(std::map<int32_t, void* > *bytes_by_server) {

    for (auto &server_bytes : (*bytes_by_server)) {
      int32_t server_id = server_bytes.first;
      uint8_t *mem = reinterpret_cast<uint8_t*>(server_bytes.second);
      int32_t &num_rows = *(reinterpret_cast<int32_t*>(mem));
      num_rows = 0;

      auto server_buff_iter = buffer_map_.find(server_id);

      CHECK(server_buff_iter != buffer_map_.end());

      std::vector<SerializedOpLogBuffer*> &buff_vec = server_buff_iter->second;

      size_t mem_offset = sizeof(int32_t);

      for (auto &buff : buff_vec) {
        memcpy(mem + mem_offset, buff->get_mem(), buff->get_size());
        num_rows += buff->get_num_row_oplogs();
        mem_offset += buff->get_size();

        total_accum_size_ -= buff->get_size();
        total_accum_num_rows_ -= buff->get_num_row_oplogs();
        delete buff;
      }
      buffer_map_.erase(server_id);
    }
  }

private:
  const bool dense_serialize_;
  const int32_t my_comm_channel_idx_;
  std::unordered_map<int32_t, std::vector<SerializedOpLogBuffer*> >
  buffer_map_;
  size_t total_accum_size_;
  size_t total_accum_num_rows_;
};

}
