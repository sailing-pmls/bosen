// author: jinliang

#pragma once

#include <vector>
#include <pthread.h>
#include <boost/unordered_map.hpp>
#include <petuum_ps_common/include/table.hpp>
#include <petuum_ps_common/include/configs.hpp>
#include <petuum_ps_common/include/abstract_row.hpp>
#include <petuum_ps_common/include/constants.hpp>
#include <petuum_ps_common/util/vector_clock.hpp>
#include <petuum_ps/server/server_table.hpp>
#include <petuum_ps/thread/ps_msgs.hpp>

namespace petuum {
struct ServerRowRequest {
public:
  int32_t bg_id; // requesting bg thread id
  int32_t table_id;
  int32_t row_id;
  int32_t clock;
};

// 1. Manage the table storage on server;
// 2. Manage the pending reads;
// 3. Manage the vector clock for clients
// 4. (TODO): manage OpLogs that need acknowledgements

class Server {
public:
  Server();
  ~Server();

  void Init(int32_t server_id,
            const std::vector<int32_t> &bg_ids);

  void CreateTable(int32_t table_id, TableInfo &table_info);
  ServerRow *FindCreateRow(int32_t table_id, int32_t row_id);
  bool ClockUntil(int32_t bg_id, int32_t clock);
  void AddRowRequest(int32_t bg_id, int32_t table_id, int32_t row_id,
    int32_t clock);
  void GetFulfilledRowRequests(std::vector<ServerRowRequest> *requests);
  void ApplyOpLogUpdateVersion(
      const void *oplog, size_t oplog_size, int32_t bg_thread_id,
      uint32_t version);
  int32_t GetMinClock();
  int32_t GetBgVersion(int32_t bg_thread_id);

  typedef void (*PushMsgSendFunc)(int32_t bg_id, ServerPushRowMsg *msg,
                                  bool is_last, int32_t version,
                                  int32_t server_min_clock);
  size_t CreateSendServerPushRowMsgs(PushMsgSendFunc PushMsgSender,
                                     bool clock_changed = true);

  size_t CreateSendServerPushRowMsgsPartial(
      PushMsgSendFunc PushMsgSend);

  bool AccumedOpLogSinceLastPush();

private:
  VectorClock bg_clock_;

  boost::unordered_map<int32_t, ServerTable> tables_;
  // mapping <clock, table id> to an array of read requests
  std::map<int32_t,
    boost::unordered_map<int32_t,
      std::vector<ServerRowRequest> > > clock_bg_row_requests_;

  // latest oplog version that I have received from a bg thread
  std::map<int32_t, uint32_t> bg_version_map_;
  // Assume a single row does not exceed this size!
  static const size_t kPushRowMsgSizeInit = 4*k1_Mi;
  size_t push_row_msg_data_size_;

  int32_t server_id_;

  size_t accum_oplog_count_;
};

}  // namespace petuum
