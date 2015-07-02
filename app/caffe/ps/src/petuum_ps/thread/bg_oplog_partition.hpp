// Author: jinliang

#pragma once

#include <stdint.h>
#include <unordered_map>
#include <map>

#include <petuum_ps/thread/context.hpp>
#include <petuum_ps_common/oplog/abstract_row_oplog.hpp>

namespace petuum {

class BgOpLogPartition : boost::noncopyable {
public:
  BgOpLogPartition(int32_t table_id, size_t update_size,
                   int32_t my_comm_channel_idx);
  ~BgOpLogPartition();

  AbstractRowOpLog *FindOpLog(int32_t row_id);
  void InsertOpLog(int32_t row_id, AbstractRowOpLog *row_oplog);
  void SerializeByServer(std::map<int32_t, void* > *bytes_by_server,
                         bool dense_serialize = false);
private:
  std::unordered_map<int32_t,  AbstractRowOpLog*> oplog_map_;
  const int32_t table_id_;
  const size_t update_size_;
  const int32_t comm_channel_idx_;
};

}   // namespace petuum
