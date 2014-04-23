// author: jinliang

#pragma once
#include "petuum_ps/oplog/row_oplog.hpp"
#include "petuum_ps/thread/ps_msgs.hpp"

#include <boost/unordered_map.hpp>
#include <vector>

namespace petuum {

typedef RowOpLog ServerRowOpLog;

class ServerOpLogTable {
public:
  ServerOpLogTable();
  ~ServerOpLogTable();

  // Bg thread of bg_id subscribes to a callback to row row_id
  // Return true if the bg thread was not registered before; otherwise
  // return false
  bool RegisterCallBack(int32_t row_id, int32_t bg_id);

  void BatchInc(int32_t row_id, const int32_t *column_ids,
    const void *deltas, int32_t num_updates);
  void SerializeByBg(
    boost::unordered_map<int32_t,ServerPushOpLogMsg* > oplog_msg_per_bg);

private:
  boost::unordered_map<int32_t, ServerRowOpLog> oplog_table_;
  boost::unordered_map<int32_t, std::vector<int32_t> > bg_callbacks_;
};
}
