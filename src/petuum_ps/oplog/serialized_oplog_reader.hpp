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

#include <boost/noncopyable.hpp>

namespace petuum {

// Provide sequential access to a byte string that's serialized oplogs.
// Used to facilicate server reading OpLogs.
// OpLogs are accessed by elements.

// OpLogs are serialized as the following memory layout
// 1. int32_t : num_tables
// 2. int32_t : table id
// 3. size_t : update_size for this table
// 4. serialized table, details in oplog_partition

class SerializedOpLogReader : boost::noncopyable {
public:
  // does not take ownership
  SerializedOpLogReader(const void *oplog_ptr):
    serialized_oplog_ptr_(reinterpret_cast<const uint8_t*>(oplog_ptr)) { }
  ~SerializedOpLogReader() {}

  bool Restart() {
    offset_ = 0;
    num_tables_left_ =
      *(reinterpret_cast<const int32_t*>(serialized_oplog_ptr_ + offset_));
    offset_ += sizeof(int32_t);
    VLOG(0) << "SerializedOpLogReader Restart(), num_tables_left = "
	    << num_tables_left_;
    if(num_tables_left_ == 0)
      return false;
    StartNewTable();
    return true;
  }

  const void *Next(int32_t *table_id, int32_t *row_id,
    int32_t const ** column_ids, int32_t *num_updates,
    bool *started_new_table) {

    // I have read all.
    if (num_tables_left_ == 0) return 0;
    *started_new_table = false;
    while(1) {
      // can read from current row
      if (num_rows_left_in_current_table_ > 0) {
        *table_id = current_table_id_;
        *row_id = *(reinterpret_cast<const int32_t*>(serialized_oplog_ptr_
          + offset_));
        offset_ += sizeof(int32_t);
        *num_updates = *(reinterpret_cast<const int32_t*>(serialized_oplog_ptr_
          + offset_));
        offset_ += sizeof(int32_t);
        *column_ids = reinterpret_cast<const int32_t*>(serialized_oplog_ptr_
          + offset_);

        const void *updates = serialized_oplog_ptr_ + offset_
          + sizeof(int32_t)*(*num_updates);
	//VLOG(0) << "update = "
	//	<< *(reinterpret_cast<const int*>(updates));
        --num_rows_left_in_current_table_;
	offset_ += (*num_updates)*(sizeof(int32_t) + update_size_);
	return updates;
      } else {
	--num_tables_left_;
	if(num_tables_left_ > 0){
	  StartNewTable();
	  *started_new_table = true;
	  continue;
	}else{
	  return 0;
	}
      }
    }
  }

private:
  void StartNewTable() {
    current_table_id_ = *(reinterpret_cast<const int32_t*>(serialized_oplog_ptr_
      + offset_));
    offset_ += sizeof(int32_t);

    update_size_ = *(reinterpret_cast<const size_t*>(serialized_oplog_ptr_
      + offset_));
    offset_ += sizeof(size_t);

    num_rows_left_in_current_table_ =
      *(reinterpret_cast<const int32_t*>(serialized_oplog_ptr_ + offset_));
    offset_ += sizeof(int32_t);

    VLOG(0) << "current_table_id = " << current_table_id_
	    << " update_size = " << update_size_
	    << " rows_left_in_current_table_ = "
	    << num_rows_left_in_current_table_;
  }

  const uint8_t *serialized_oplog_ptr_;
  size_t update_size_;
  int32_t offset_; // bytes to be read next
  int32_t num_tables_left_; // number of tables that I have not finished
                            //reading (might have started)
  int32_t current_table_id_;
  int32_t num_rows_left_in_current_table_;
};


class OpLogSerializer : boost::noncopyable {
public:
  OpLogSerializer() {}
  ~OpLogSerializer() {}

  size_t Init(const std::map<int32_t, size_t> &table_size_map) {
    num_tables_ = table_size_map.size();
    // space for num of tables
    size_t total_size = sizeof(int32_t);
    for(auto iter = table_size_map.cbegin(); iter != table_size_map.cend();
      iter++){
      int32_t table_id = iter->first;
      size_t table_size = iter->second;
      offset_map_[table_id] = total_size;
      // next table is offset by
      // 1) the current table size and
      // 2) space for table id
      // 3) update size
      total_size += table_size + sizeof(int32_t) + sizeof(size_t);
    }
    return total_size;
  }

  // does not take ownership
  void AssignMem(void *mem) {
    mem_ = reinterpret_cast<uint8_t*>(mem);
    *(reinterpret_cast<int32_t*>(mem_)) = num_tables_;
  }

  void *GetTablePtr(int32_t table_id) {
    return mem_ + offset_map_[table_id];
  }

private:
  std::map<int32_t, size_t> offset_map_;
  uint8_t *mem_;
  int32_t num_tables_;
};

}  // namespace petuum
