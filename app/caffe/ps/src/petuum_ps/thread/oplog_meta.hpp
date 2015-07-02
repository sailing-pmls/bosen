#pragma once

#include <map>
#include <utility>
#include <boost/noncopyable.hpp>
#include <glog/logging.h>

#include <petuum_ps/thread/context.hpp>
#include <petuum_ps/thread/naive_table_oplog_meta.hpp>
#include <petuum_ps/thread/random_table_oplog_meta.hpp>
#include <petuum_ps/thread/random_table_oplog_meta_dense.hpp>
#include <petuum_ps/thread/fixed_table_oplog_meta_dense.hpp>
#include <petuum_ps/thread/value_table_oplog_meta.hpp>
#include <petuum_ps/thread/value_table_oplog_meta_approx.hpp>

namespace petuum {

class OpLogMeta : boost::noncopyable {
public:
  OpLogMeta() { }

  ~OpLogMeta() {
    for(auto &oplog_pair : table_oplog_map_) {
      delete oplog_pair.second;
    }
  }

  AbstractTableOpLogMeta *AddTableOpLogMeta(int32_t table_id,
                                            const AbstractRow *sample_row,
                                            size_t table_size) {
    AbstractTableOpLogMeta *table_oplog_meta;

    if (GlobalContext::get_naive_table_oplog_meta()) {
      table_oplog_meta = new NaiveTableOpLogMeta(sample_row);
    } else {
      UpdateSortPolicy update_sort_policy
          = GlobalContext::get_update_sort_policy();

      switch(update_sort_policy) {
        case FixedOrder:
          {
            if (table_size > 0) {
              table_oplog_meta
                  = new FixedTableOpLogMetaDense(sample_row, table_size);
            } else {
              table_oplog_meta
                  = new RandomTableOpLogMeta(sample_row);
            }
          }
          break;
        case Random:
          {
            if (table_size > 0) {
              table_oplog_meta
                  = new RandomTableOpLogMetaDense(sample_row, table_size);
            } else {
              table_oplog_meta
                  = new RandomTableOpLogMeta(sample_row);
            }
          }
          break;
        case RelativeMagnitude:
          {
            if (table_size == 0 || GlobalContext::get_use_aprox_sort()) {
              table_oplog_meta
                  = new ValueTableOpLogMetaApprox(sample_row);
            } else {
              table_oplog_meta
                  = new ValueTableOpLogMeta(sample_row, table_size);
            }
          }
          break;
        default:
          LOG(FATAL) << "Unsupported update_sort_policy = "
                     << update_sort_policy;
      }

    }

    table_oplog_map_.insert(
        std::make_pair(table_id, table_oplog_meta));

    return table_oplog_meta;
  }

  AbstractTableOpLogMeta *Get(int32_t table_id) {
    auto oplog_iter = table_oplog_map_.find(table_id);
    if (oplog_iter == table_oplog_map_.end())
      return 0;
    return oplog_iter->second;
  }

  bool OpLogMetaExists() const {
    for (auto oplog_iter = table_oplog_map_.begin();
         oplog_iter != table_oplog_map_.end(); ++oplog_iter) {
      if (oplog_iter->second->GetNumRowOpLogs() > 0)
        return true;
    }
    return false;
  }

private:
  std::map<int32_t, AbstractTableOpLogMeta*> table_oplog_map_;
};

}
