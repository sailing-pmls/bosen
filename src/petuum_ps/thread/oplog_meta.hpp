#pragma once

#include <map>
#include <utility>
#include <boost/noncopyable.hpp>
#include <glog/logging.h>

#include <petuum_ps/thread/table_oplog_meta.hpp>

namespace petuum {

class OpLogMeta : boost::noncopyable {
public:
  OpLogMeta() { }

  ~OpLogMeta() { }

  TableOpLogMeta *AddTableOpLogMeta(int32_t table_id,
                                    const AbstractRow *sample_row) {
    table_oplog_map_.insert(
        std::make_pair(table_id, TableOpLogMeta(sample_row)));

    auto oplog_iter = table_oplog_map_.find(table_id);

    return &(oplog_iter->second);
  }

  TableOpLogMeta *Get(int32_t table_id) {
    auto oplog_iter = table_oplog_map_.find(table_id);
    if (oplog_iter == table_oplog_map_.end())
      return 0;
    return &(oplog_iter->second);
  }

  bool OpLogMetaExists() const {
    for (auto oplog_iter = table_oplog_map_.begin();
         oplog_iter != table_oplog_map_.end(); ++oplog_iter) {
      if (oplog_iter->second.GetNumRowOpLogs() > 0)
        return true;
    }
    return false;
  }

private:
  std::map<int32_t, TableOpLogMeta> table_oplog_map_;
};

}
