// Author: jinliang

#pragma once

#include <stdint.h>
#include <boost/unordered_map.hpp>
#include <map>

#include "petuum_ps/thread/context.hpp"
#include "petuum_ps/oplog/row_oplog.hpp"

namespace petuum {

class BgOpLogPartition : boost::noncopyable {
public:
  BgOpLogPartition();
  BgOpLogPartition(int32_t table_id, size_t update_size);
  ~BgOpLogPartition();

  RowOpLog *FindOpLog(int32_t row_id);
  void InsertOpLog(int32_t row_id, RowOpLog *row_oplog);
  void SerializeByServer(std::map<int32_t, void* > *bytes_by_server);

private:
  boost::unordered_map<int32_t,  RowOpLog*> oplog_map_;
  int32_t table_id_;
  size_t update_size_;
};

}   // namespace petuum
