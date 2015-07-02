// author: jinliang

#include <boost/noncopyable.hpp>
#include <petuum_ps/server/server_table.hpp>
#include <petuum_ps/thread/context.hpp>


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
  SerializedOpLogReader(
      const void *oplog_ptr,
      const boost::unordered_map<int32_t, ServerTable> &server_tables):
      serialized_oplog_ptr_(reinterpret_cast<const uint8_t*>(oplog_ptr)),
      server_tables_(server_tables) { }
  ~SerializedOpLogReader() {}

  bool Restart() {
    offset_ = 0;
    num_tables_left_ =
      *(reinterpret_cast<const int32_t*>(serialized_oplog_ptr_ + offset_));
    offset_ += sizeof(int32_t);
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
        *row_id = *(reinterpret_cast<const int32_t*>(
            serialized_oplog_ptr_ + offset_));
        offset_ += sizeof(int32_t);
        size_t serialized_size;
        const void *update
            = GetNextUpdate_(curr_sample_row_oplog_,
                             serialized_oplog_ptr_ + offset_,
                             column_ids, num_updates, &serialized_size);
        offset_ += serialized_size;
        --num_rows_left_in_current_table_;
        return update;
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
  typedef const void *(*GetNextUpdateFunc)(
      const AbstractRowOpLog* sample_row_oplog, const void *mem,
      int32_t const ** column_ids, int32_t *num_updates,
      size_t *serialized_size);

  static const void *GetNextUpdateSparse(
      const AbstractRowOpLog* sample_row_oplog, const void *mem,
      int32_t const ** column_ids, int32_t *num_updates,
      size_t *serialized_size) {
    return sample_row_oplog->ParseSparseSerializedOpLog(
        mem, column_ids, num_updates, serialized_size);
  }

  static const void *GetNextUpdateDense(
      const AbstractRowOpLog* sample_row_oplog, const void *mem,
      int32_t const ** column_ids, int32_t *num_updates,
      size_t *serialized_size) {
    return sample_row_oplog->ParseDenseSerializedOpLog(
        mem, num_updates, serialized_size);
  }

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

    auto table_iter = server_tables_.find(current_table_id_);

    CHECK(table_iter != server_tables_.end())
        << "table_id = " << current_table_id_
        << " not found!"
        << " num_tables_left = " << num_tables_left_
        << " num_rows_left_in_current_table_ = "
        << num_rows_left_in_current_table_
        << " update_size = " << update_size_;

    //    LOG(INFO) << "thread id = " << ThreadContext::get_id();
    //        << " table_id = " << current_table_id_
    //        << " num_tables_left = " << num_tables_left_
    //        << " num_rows_left_in_current_table_ = "
    //        << num_rows_left_in_current_table_
    //        << " update_size = " << update_size_;


    curr_sample_row_oplog_ = table_iter->second.get_sample_row_oplog();
    if (table_iter->second.oplog_dense_serialized())
      GetNextUpdate_ = GetNextUpdateDense;
    else
      GetNextUpdate_ = GetNextUpdateSparse;
  }

  const uint8_t *serialized_oplog_ptr_;
  size_t update_size_;
  int32_t offset_; // bytes to be read next
  int32_t num_tables_left_; // number of tables that I have not finished
                            //reading (might have started)
  int32_t current_table_id_;
  int32_t num_rows_left_in_current_table_;

  const boost::unordered_map<int32_t, ServerTable> &server_tables_;
  const AbstractRowOpLog *curr_sample_row_oplog_;
  GetNextUpdateFunc GetNextUpdate_;
};

}  // namespace petuum
