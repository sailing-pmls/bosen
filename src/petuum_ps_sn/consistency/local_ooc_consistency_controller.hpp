#pragma once

#include <petuum_ps_sn/consistency/local_consistency_controller.hpp>

#include <utility>
#include <vector>
#include <cstdint>
#include <atomic>
#include <string>
#include <leveldb/db.h>
#include <set>
#include <mutex>

namespace petuum {

class LocalOOCConsistencyController : public LocalConsistencyController {
public:
  LocalOOCConsistencyController(const ClientTableConfig& config,
    int32_t table_id,
    ProcessStorage& process_storage,
    const AbstractRow* sample_row,
    boost::thread_specific_ptr<ThreadTableSN> &thread_cache);

  ~LocalOOCConsistencyController();

protected:
  void MakeOOCDBPath(std::string *db_path);
  virtual void CreateInsertRow(int32_t row_id, RowAccessor *row_accessor);

  std::mutex create_row_mtx_;
  leveldb::DB* db_;
  std::set<int32_t> on_disk_row_index_;
};

}  // namespace petuum
