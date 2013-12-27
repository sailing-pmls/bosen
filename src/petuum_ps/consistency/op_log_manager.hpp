// Copyright (c) 2013, SailingLab
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

#ifndef PETUUM_OP_LOG_MANAGER
#define PETUUM_OP_LOG_MANAGER

#include "petuum_ps/storage/lru_row_storage.hpp"
#include "petuum_ps/include/configs.hpp"
#include "petuum_ps/proxy/client_proxy.hpp"
#include "petuum_ps/proxy/table_partitioner.hpp"

#include <vector>
#include <boost/unordered_map.hpp>
#include <boost/shared_array.hpp>
#include <boost/optional.hpp>

namespace petuum {

// EntryOp represent an operation on an entry. It only records the
// operation and the associated value. It does not record Row ID and column
// ID, which are recorded in OpLogTable.
//
// TODO(wdai): Consider using Protobuf to serialize OpLog more compactly.
// Caution about template type.
template<typename V>
struct EntryOp {
  // Operations on entry.
  enum OpType {
    NO_OP,   // Needed for default constructor.
    PUT,
    INC
  };

  OpType op;

  // Since the data type V is often POD, e.g., double and int, we directly store
  // it as val.
  V val;

  // Default constructor.
  EntryOp() : op(NO_OP) { }
};

// EntryOpExtended is the format to be serialized to server.
template<typename V>
struct EntryOpExtended : public EntryOp<V> {
  // Store row and col ID so server can apply it without additional info.
  int32_t row_id;
  int32_t col_id;

  EntryOpExtended() { }

  // "Copy constructor"
  EntryOpExtended(const EntryOp<V>& entry_op_internal) :
    EntryOp<V>(entry_op_internal) { }
};


// OpLogTableManager maintains OpLogTable and a ThreadCache to provide
// read-my-write. When a write comes, apply it to the ThreadCache (if the row
// is found in thread_cache_) and record it in OpLogTable. Upon a read
// request, search for the row in thread_cache_; return the value if found,
// otherwise return oplog associated with that entry / row.
//
// Comment(wdai): We do not provide HasRow() for the thread_cache_ since
// ConsistencyController should just use Get() to save one map lookup.
template<template<typename> class ROW, typename V>
class OpLogManager {
  public:
    // TODO(wdai): table_id is part of op_log_config, no need for additional
    // parameter.
    OpLogManager(const OpLogManagerConfig& op_log_config, 
        int32_t table_id);

    // Getters and setters.
    int get_max_pending_op_logs() const;
    void set_max_pending_op_logs(int max_pending_op_logs_);

    // Read an entry of the table. Return 1 and val if found in
    // thread_cache_; 0 otherwise; <0 for errors.
    int Get(int32_t row_id, int32_t col_id, V* val);

    // Read an entry of the table with the iteration number associated with
    // the row (row_iter). Return 1 and val if found in thread_cache_; 0
    // otherwise; <0 for errors.
    int Get(int32_t row_id, int32_t col_id, V* val, int32_t *row_iter);

    // Return 1 and row if found; 0 otherwise.
    //int GetRow(int32_t row_id, ROW* row) const;
    int GetRow(int32_t row_id, ROW<V>* row);

    // Get a pointer to row_id which is valid while the row is still in
    // OpLogManager.
    ROW<V>* GetRowUnsafe(int32_t row_id, int32_t *row_iter);

    // If the ConsistencyController cannot find the value in Get(), it uses
    // GetOpLog and read from processor cache (or server if necessary). Return
    // 1 and oplog if OpLog is found for the entry; 0 otherwise.
    int GetOpLog(int32_t row_id, int32_t col_id, EntryOp<V>* oplog) const;

    // Two entry-wide write methods. It records it in op_log_table_ by
    // coalescing the change with any existing oplog for the entry;
    // additionally, it applies the change to thread_cache_ if row_id is
    // present. Return 0 on success, and 1 if num_pending_op_logs_ hits
    // max_pending_op_logs_ so the user can flush the oplog.
    //
    // New EntryOp is coalesced according to:
    // Inc(a) + Inc(b) = Inc(a+b)
    // Inc(a) + Put(b) = Put(b)
    // Put(a) + Put(b) = Put(b)
    // Put(a) + Inc(b) = Put(a + b).
    int Inc(int32_t row_id, int32_t col_id, V delta);
    int Put(int32_t row_id, int32_t col_id, V new_val);

    // When ConsistencyController read from the processor cache or server, it
    // puts it calls InsertThreadCache(). Immediately apply pending oplog to
    // row to support read-my-write.
    void InsertThreadCache(int32_t row_id, const ROW<V>& row);

    // Serialize everything in op_log_table_. Consistency will send it to
    // server. Return the number of bytes, or negatives for errors.
    int SerializeOpLogTable(boost::shared_array<uint8_t>* out_bytes);

    // Serialize oplog table into partitions. out_bytes_by_server and
    // num_bytes_by_server both have length num_servers_.
    // num_bytes_by_server[i] is the number of bytes in
    // out_bytes_by_server[i]. num_bytes_by_server[i] == 0 iff there is no
    // oplog associated with server i.
    void SerializeOpLogTableByServer(
        std::vector<boost::shared_array<uint8_t> >* out_bytes_by_server,
        std::vector<int>* num_bytes_by_server);

    // When OpLog is applied to server, remove all oplog for a table.
    void ClearOpLogTable();

    // ==== Type Declarations ====
    // Map from column ID (int32_t) to EntryOp.
    typedef typename boost::unordered_map<int32_t, EntryOp<V> > OpLogRow;

    // Map from row Id to OpLogRow.
    typedef boost::unordered_map<int32_t, OpLogRow> OpLogTable;

    // Use LRU cache as thread cache.
    typedef LRURowStorage<ROW, V> ThreadCache;

    // Simplify verbose derived type syntax.
    typedef typename EntryOp<V>::OpType OpType;

  private:  // private member functions.
    // Insert an entry into the oplog. Return >0 if we hit
    // max_pending_op_logs_, 0 if not, <0 for errors.
    int InsertOpLog(int32_t row_id, int32_t col_id, OpType op_type, V val);

    // Apply op logs in op_log_table_ to row_id in thread_cache_ to achieve
    // read-my-write.
    void ApplyOpLogToThreadCacheRow(int32_t row_id);

  private:  // private member variables.
    // OpLogTable is a two level map that allows access to EntryOps
    // associated with a particular entry identified by (row_id, col_id).  As
    // an optimization, When an EntryOp is added to that entry, we coalesce
    // them.
    OpLogTable op_log_table_;

    ThreadCache thread_cache_;

    // # of pending EntryOp. This is used to keep OpLog size small.
    int32_t num_pending_op_logs_;

    // Number of servers. Needed when serializing oplog for each server.
    int32_t num_servers_;

    int32_t table_id_;

    // The maximum number of op_logs.
    int max_pending_op_logs_;
};

// Deserialization.
template<typename V>
int DeserializeOpLogs(const boost::shared_array<uint8_t>& op_log_bytes,
    std::vector<EntryOpExtended<V> >* out);

//======================= Implementation of OpLogManager ======================

template<template<typename> class ROW, typename V>
OpLogManager<ROW, V>::OpLogManager(const OpLogManagerConfig& op_log_config,
    int32_t table_id) :
  thread_cache_(op_log_config.thread_cache_capacity),
  num_pending_op_logs_(0),
  max_pending_op_logs_(op_log_config.max_pending_op_logs),
  table_id_(op_log_config.table_id) {
  CHECK_GT(op_log_config.num_servers, 0)
    << "There needs to be at least 1 server.";
  num_servers_ = op_log_config.num_servers;

  if (op_log_config.thread_cache_capacity == 0) {
    LOG(WARNING) << "Use thread cache size = 0";
  }
  VLOG(0) << "OpLogManager created with thread_cache_capacity = "
    << op_log_config.thread_cache_capacity;
}

template<template<typename> class ROW, typename V>
int OpLogManager<ROW, V>::get_max_pending_op_logs() const {
  return max_pending_op_logs_;
}

template<template<typename> class ROW, typename V>
void OpLogManager<ROW,V>::set_max_pending_op_logs(int max_pending_op_logs_) {
  max_pending_op_logs_ = max_pending_op_logs_;
}

template<template<typename> class ROW, typename V>
int OpLogManager<ROW, V>::Get(int32_t row_id, int32_t col_id, V* val) {
  return thread_cache_.Get(row_id, col_id, val);
}

template<template<typename> class ROW, typename V>
int OpLogManager<ROW, V>::Get(int32_t row_id, int32_t col_id, V* val,
    int32_t *row_iter) {

  int ret = thread_cache_.Get(row_id, col_id, val, row_iter);

  return ret;
}

template<template<typename> class ROW, typename V>
int OpLogManager<ROW, V>::GetRow(int32_t row_id, ROW<V>* row) {
  return thread_cache_.GetRow(row_id, row);
}

template<template<typename> class ROW, typename V>
ROW<V>* OpLogManager<ROW, V>::GetRowUnsafe(int32_t row_id, int32_t *row_iter) {
  boost::optional<ROW<V>&> row_ref = thread_cache_.GetRowUnsafe(row_id, row_iter);
  if (row_ref) return &(*row_ref);
  return NULL;
}

template<template<typename> class ROW, typename V>
int OpLogManager<ROW, V>::GetOpLog(int32_t row_id, int32_t col_id,
    EntryOp<V>* oplog) const {
  if (op_log_table_.count(row_id) > 0) {
    try {
      // Use "at" to get const reference as return value.
      const OpLogRow& op_log_row = op_log_table_.at(row_id);
      if (op_log_row.count(col_id) > 0) {
        // Found it!
        *oplog = op_log_row.at(col_id);
        return 1;
      }
    } catch (std::out_of_range e) {
      LOG(FATAL) << "Should not reach here. Report bug.";
    }
  }
  return 0;
}

template<template<typename> class ROW, typename V>
int OpLogManager<ROW, V>::Inc(int32_t row_id, int32_t col_id, V delta) {
  // Don't care if row_id is in the thread_cache_ (return >0) or not (return
  // 0).
  CHECK_GE(thread_cache_.Inc(row_id, col_id, delta), 0);
  return InsertOpLog(row_id, col_id, OpType::INC, delta);
}

template<template<typename> class ROW, typename V>
int OpLogManager<ROW, V>::Put(int32_t row_id, int32_t col_id, V new_val) {
  // Don't care if row_id is in the thread_cache_ (return >0) or not (return
  // 0).
  CHECK_GE(thread_cache_.Put(row_id, col_id, new_val), 0);
  return InsertOpLog(row_id, col_id, OpType::PUT, new_val);
}

template<template<typename> class ROW, typename V>
int OpLogManager<ROW, V>::InsertOpLog(int32_t row_id, int32_t col_id,
    OpType op_type, V val) {
  // Implicitly insert a new row if row_id did not already exist.
  OpLogRow& op_log_row = op_log_table_[row_id];

  if (op_log_row.count(col_id) > 0) {
    // We found existing entry op for (row_id, col_id). Coalesce it with new
    // oplog.
    EntryOp<V>& entry_op = op_log_row[col_id];
    switch (entry_op.op) {
      // Existing op is an INC
      case OpType::INC:
        switch (op_type) {
          case OpType::INC:
            // Inc(a) + Inc(b) = Inc(a+b)
            entry_op.val += val;
            break;
          case OpType::PUT:
            // Inc(a) + Put(b) = Put(b)
            entry_op.op = OpType::PUT;
            entry_op.val = val;
            break;
          default:
            LOG(FATAL) << "Incoming op should not be no-op. Report bug.";
        }
        break;

        // Existing op is a PUT.
      case OpType::PUT:
        switch (op_type) {
          case OpType::INC:
            // Put(a) + Inc(b) = Put(a+b)
            entry_op.val += val;
            break;
          case OpType::PUT:
            // Put(a) + Put(b) = Put(b)
            entry_op.val = val;
            break;
          default:
            LOG(FATAL) << "Incoming op should not be no-op. Report bug.";
        }
        break;

      default:
        LOG(FATAL) << "OpLog should not contain no-op. Report bug.";
    }
    // Even though we didn't add new oplog, still complain if there are too
    // many pending op logs.
    return (num_pending_op_logs_ >= max_pending_op_logs_) ? 1 : 0;;
  }
  // No existing oplog for column col_id. Insert a new one column.
  EntryOp<V>& new_op = op_log_row[col_id];
  new_op.op = op_type;
  new_op.val = val;

  // Now check if we hit max_pending_op_logs_
  return (++num_pending_op_logs_ >= max_pending_op_logs_) ? 1 : 0;
}

template<template<typename> class ROW, typename V>
void OpLogManager<ROW, V>::InsertThreadCache(int32_t row_id,
    const ROW<V>& row) {
  CHECK_GE(thread_cache_.PutRow(row_id, row), 0); // >0 on success.
  ApplyOpLogToThreadCacheRow(row_id);
}

template<template<typename> class ROW, typename V>
void OpLogManager<ROW, V>::ApplyOpLogToThreadCacheRow(int32_t row_id) {
  //LOG_FIRST_N(INFO, 5) << "ApplyOpLogToThreadCacheRow called";
  if (op_log_table_.count(row_id) == 0 || !thread_cache_.HasRow(row_id)) {
    // Nothing to do.
    return;
  }

  // Since thread_cache_.HasRow(row_id), this must be a valid reference.
  ROW<V>& row_ref = *thread_cache_.GetRowInternal(row_id);
  //return ;

  // Both row row_id is present in op log and thread_cache_. Iterate through
  // op log
  OpLogRow& op_log_row = op_log_table_[row_id];
  for (typename OpLogRow::const_iterator it = op_log_row.begin();
      it != op_log_row.end(); ++it) {
    int32_t col_id = it->first;
    const EntryOp<V>& entry_op = it->second;
    switch (entry_op.op) {
      case OpType::PUT:
        row_ref[col_id] = entry_op.val;
        break;
      case OpType::INC:
        row_ref[col_id] += entry_op.val;
        break;
      default:
        LOG(FATAL) << "OpLog should not contain no-op. Report bug.";
    }
  }
}

template<template<typename> class ROW, typename V>
int OpLogManager<ROW, V>::SerializeOpLogTable(
    boost::shared_array<uint8_t>* out_bytes) {
  // Add sizeof(int32_t) to prepend num_pending_op_logs_.
  int n_bytes = sizeof(EntryOpExtended<V>) * num_pending_op_logs_
    + sizeof(int);
  uint8_t *data_ptr = new uint8_t[n_bytes];
  int32_t *num_op_logs = reinterpret_cast<int32_t*>(data_ptr);
  *num_op_logs = num_pending_op_logs_;
  int32_t i_op_log = 0;
  VLOG(3) << "Serializing Oplog...";
  for (typename OpLogTable::const_iterator row_it = op_log_table_.begin();
      row_it != op_log_table_.end(); ++row_it) {
    int32_t row_id = row_it->first;
    const OpLogRow& op_log_row = row_it->second;
    for (typename OpLogRow::const_iterator col_it = op_log_row.begin();
        col_it != op_log_row.end(); ++col_it) {
      // Copy entry_op to entry_op_extended.
      int32_t col_id = col_it->first;
      const EntryOp<V>& entry_op = col_it->second;
      EntryOpExtended<V>* entry_op_extended =
        reinterpret_cast<EntryOpExtended<V>*>(data_ptr
            + i_op_log * sizeof(EntryOpExtended<V>) + sizeof(int32_t));
      // Use "copy constructor" for assignment.
      *entry_op_extended = entry_op;
      entry_op_extended->row_id = row_id;
      entry_op_extended->col_id = col_id;
      ++i_op_log;
      VLOG(3) << "(row_id, col_id) = (" << row_id << ", " << col_id
        << "). Op=" << (entry_op.op == EntryOp<V>::PUT ? "PUT" : "INC")
        << " Val=" << entry_op.val;
    }
  }
  CHECK_EQ(num_pending_op_logs_, i_op_log);
  out_bytes->reset(data_ptr);

  // Iterate through two levels of map.
  return n_bytes;
}

  // TODO: This is wrong, you cannot assume server ids are a consecutive set of integers

template<template<typename> class ROW, typename V>
void OpLogManager<ROW, V>::SerializeOpLogTableByServer(
    std::vector<boost::shared_array<uint8_t> >* out_bytes_by_server,
    std::vector<int>* num_bytes_by_server) {
  // Figure out how many oplog entries are in each partition in order to
  // allocate memory.
  TablePartitioner& partitioner = TablePartitioner::GetInstance();
  std::vector<int32_t> num_oplogs_by_server(num_servers_);
  for (typename OpLogTable::const_iterator row_it = op_log_table_.begin();
      row_it != op_log_table_.end(); ++row_it) {
    int32_t row_id = row_it->first;
    int32_t server_id = partitioner.GetRowAssignment(table_id_, row_id);
    const OpLogRow& op_log_row = row_it->second;
    for (typename OpLogRow::const_iterator col_it = op_log_row.begin();
        col_it != op_log_row.end(); ++col_it) {
      ++num_oplogs_by_server[server_id];
    }
  }

  // Now serializing.
  out_bytes_by_server->resize(num_servers_);
  num_bytes_by_server->resize(num_servers_);
  for (int i = 0; i < num_servers_; ++i) {
    (*num_bytes_by_server)[i] =
      sizeof(EntryOpExtended<V>) * num_oplogs_by_server[i] + sizeof(int);
    (*out_bytes_by_server)[i].reset(new uint8_t[(*num_bytes_by_server)[i]]);
    int32_t *num_op_logs =
      reinterpret_cast<int32_t*>((*out_bytes_by_server)[i].get());
    *num_op_logs = num_oplogs_by_server[i];
  }


  // oplog_counter_by_server keeps track of the pointer to add the next oplog.
  std::vector<int32_t> oplog_counter_by_server(num_servers_);
  for (typename OpLogTable::const_iterator row_it = op_log_table_.begin();
      row_it != op_log_table_.end(); ++row_it) {
    int32_t row_id = row_it->first;
    int32_t server_id = partitioner.GetRowAssignment(table_id_, row_id);
    const OpLogRow& op_log_row = row_it->second;
    for (typename OpLogRow::const_iterator col_it = op_log_row.begin();
        col_it != op_log_row.end(); ++col_it) {
      // Copy entry_op to entry_op_extended.
      int32_t col_id = col_it->first;
      const EntryOp<V>& entry_op = col_it->second;
      EntryOpExtended<V>* entry_op_extended =
        reinterpret_cast<EntryOpExtended<V>*>(
            (*out_bytes_by_server)[server_id].get()
            + oplog_counter_by_server[server_id] * sizeof(EntryOpExtended<V>)
            + sizeof(int32_t));
      // Use "copy constructor" for assignment.
      *entry_op_extended = entry_op;
      entry_op_extended->row_id = row_id;
      entry_op_extended->col_id = col_id;
      ++oplog_counter_by_server[server_id];
      VLOG(3) << "(row_id, col_id) = (" << row_id << ", " << col_id
        << "). Op=" << (entry_op.op == EntryOp<V>::PUT ? "PUT" : "INC")
        << " Val=" << entry_op.val;
    }
  }
}

template<typename V>
int DeserializeOpLogs(const boost::shared_array<uint8_t>& op_log_bytes,
    std::vector<EntryOpExtended<V> >* out) {
  // The first sizeof(int32_t) bytes denotes number of EntryOpExtended
  uint8_t *data_ptr = op_log_bytes.get();
  int32_t num_op_logs = *reinterpret_cast<int32_t*>(data_ptr);
  out->resize(num_op_logs);
  for (int i = 0; i < num_op_logs; ++i) {
    (*out)[i] = *reinterpret_cast<EntryOpExtended<V>*>(
        data_ptr + i * sizeof(EntryOpExtended<V>) + sizeof(int32_t));
  }
  return 0;
}

template<template<typename> class ROW, typename V>
void OpLogManager<ROW, V>::ClearOpLogTable() {
  op_log_table_.clear();
  num_pending_op_logs_ = 0;
}

}  // namespace petuum

#endif  // PETUUM_OP_LOG_MANAGER
