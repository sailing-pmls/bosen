#pragma once

#include <map>
#include <boost/noncopyable.hpp>
#include <glog/logging.h>

#include "petuum_ps/thread/bg_oplog_partition.hpp"

namespace petuum {

class BgOpLog : boost::noncopyable {
public:
  BgOpLog(){}

  ~BgOpLog() {
    //VLOG(0) << "Destroy BgOpLog";
    for (auto iter = table_oplog_map_.begin(); iter != table_oplog_map_.end();
      iter++) {
      delete iter->second;
    }
  }
  // takes ownership of the oplog partition
  void Add(int32_t table_id, BgOpLogPartition *bg_oplog_partition_ptr){
    table_oplog_map_[table_id] = bg_oplog_partition_ptr;
  }

  BgOpLogPartition* Get(int32_t table_id) const {
    return table_oplog_map_.at(table_id);
  }
private:
  std::map<int32_t, BgOpLogPartition*> table_oplog_map_;

};

}
