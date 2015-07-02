// author: jinliang
#pragma once

#include <vector>
#include <stdint.h>
#include <pthread.h>

#include <petuum_ps/server/server.hpp>
#include <petuum_ps_common/util/thread.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps_common/thread/msg_tracker.hpp>
#include <petuum_ps_common/util/max_vector_clock.hpp>
#include <petuum_ps_common/include/constants.hpp>

namespace petuum {
class ServerThread : public Thread {
public:
  ServerThread(int32_t my_id, pthread_barrier_t *init_barrier):
      my_id_(my_id),
      bg_worker_ids_(GlobalContext::get_num_clients()),
      num_shutdown_bgs_(0),
      comm_bus_(GlobalContext::comm_bus),
      init_barrier_(init_barrier),
      msg_tracker_(kMaxPendingMsgs),
      pending_clock_push_row_(false),
      pending_shut_down_(false),
      min_table_staleness_(INT_MAX) { }

  virtual ~ServerThread() { }

  void ShutDown() {
    Join();
  }

protected:
  static bool WaitMsgBusy(int32_t *sender_id, zmq::message_t *zmq_msg,
                          long timeout_milli = -1);
  static bool WaitMsgSleep(int32_t *sender_id, zmq::message_t *zmq_msg,
                           long timeout_milli  = -1);
  static bool WaitMsgTimeOut(int32_t *sender_id, zmq::message_t *zmq_msg,
                             long timeout_milli);
  CommBus::WaitMsgTimeOutFunc WaitMsg_;

  virtual void InitWhenStart();

  virtual void SetWaitMsg();

  virtual void InitServer();
  virtual void ServerPushRow() { }
  virtual void RowSubscribe(ServerRow *server_row, int32_t client_id) { }

  void SetUpCommBus();
  void ConnectToNameNode();
  int32_t GetConnection(bool *is_client, int32_t *client_id);

  void SendToAllBgThreads(MsgBase *msg);
  bool HandleShutDownMsg();
  void HandleCreateTable(int32_t sender_id, CreateTableMsg &create_table_msg);
  void HandleRowRequest(int32_t sender_id, RowRequestMsg &row_request_msg);
  void ReplyRowRequest(int32_t bg_id, ServerRow *server_row,
                       int32_t table_id, int32_t row_id, int32_t server_clock,
                       uint32_t version);
  void HandleOpLogMsg(int32_t sender_id,
                      ClientSendOpLogMsg &client_send_oplog_msg);

  virtual void HandleEarlyCommOn();
  virtual void HandleEarlyCommOff();

  virtual void HandleBgServerPushRowAck(
      int32_t bg_id, uint64_t ack_seq);

  virtual long ServerIdleWork();
  virtual long ResetServerIdleMilli();

  virtual void SendOpLogAckMsg(int32_t bg_id, uint32_t version,
                               uint64_t seq);
  void SendServerShutDownAcks();

  void ShutDownServer();

  virtual void *operator() ();

  virtual void PrepareBeforeInfiniteLoop();
  virtual void ClockNotice();

  virtual void AdjustSuppressionLevel(int32_t bg_id, int32_t bg_clock);

  int32_t my_id_;
  std::vector<int32_t> bg_worker_ids_;
  Server server_obj_;
  int32_t num_shutdown_bgs_;
  CommBus* const comm_bus_;

  pthread_barrier_t *init_barrier_;

  MsgTracker msg_tracker_;
  bool pending_clock_push_row_;
  bool pending_shut_down_;

  int32_t min_table_staleness_;

  MaxVectorClock client_progress_clock_;
};

}
