// server_threads.hpp
// author: jinliang

#pragma once

#include <vector>
#include <pthread.h>
#include <boost/thread/tss.hpp>
#include <queue>

#include "petuum_ps/server/server.hpp"
#include "petuum_ps/thread/ps_msgs.hpp"
#include "petuum_ps/thread/context.hpp"
#include "petuum_ps/comm_bus/comm_bus.hpp"

namespace petuum {

class ServerThreads {
public:
  static void Init(int32_t id_st);
  static void ShutDown();

private:
  // server context is specific to the server thread
  struct ServerContext {
    std::vector<int32_t> bg_thread_ids_;
    Server server_obj_;
    int32_t num_shutdown_bgs_;
  };

  static void *ServerThreadMain(void *server_thread_info);

  //typedef void *(*ServerThreadMainFunc)(void *);

  static void SSPPushServerPushRow();
  static void SSPServerPushRow() { }
  typedef void (*ServerPushRowFunc)();
  static ServerPushRowFunc ServerPushRow;

  typedef void (*RowSubscribeFunc)(ServerRow *server_row, int32_t client_id);
  static RowSubscribeFunc RowSubscribe;
  static void SSPRowSubscribe(ServerRow *server_row, int32_t client_id) {}
  static void SSPPushRowSubscribe(ServerRow *server_row, int32_t client_id);

  // communication function
  // assuming the caller is not name node
  static void ConnectToNameNode();
  static int32_t GetConnection(bool *is_client, int32_t *client_id);

  /**
   * Functions that operate on the particular thread's specific ServerContext.
   */
  static void SetUpServerContext();
  static void SetUpCommBus();
  static void InitServer();

  static void SendToAllBgThreads(void *msg, size_t msg_size);
  static bool HandleShutDownMsg(); // returns true if the server may shut down
  static void HandleCreateTable(int32_t sender_id,
    CreateTableMsg &create_table_msg);
  static void HandleRowRequest(int32_t sender_id,
    RowRequestMsg &row_request_msg);
  static void ReplyRowRequest(int32_t bg_id, ServerRow *server_row,
    int32_t table_id, int32_t row_id, int32_t server_clock, uint32_t version);
  static void HandleOpLogMsg(int32_t sender_id,
    ClientSendOpLogMsg &client_send_oplog_msg);

  static void SendServerPushRowMsg (int32_t bg_id, ServerPushRowMsg *msg,
                                    bool last_msg);

  static pthread_barrier_t init_barrier;
  static std::vector<pthread_t> threads_;
  static std::vector<int32_t> thread_ids_;
  static boost::thread_specific_ptr<ServerContext> server_context_;

  static CommBus::RecvFunc CommBusRecvAny;
  static CommBus::RecvTimeOutFunc CommBusRecvTimeOutAny;
  static CommBus::SendFunc CommBusSendAny;
  static CommBus::RecvAsyncFunc CommBusRecvAsyncAny;
  static CommBus::RecvWrapperFunc CommBusRecvAnyWrapper;

  static void CommBusRecvAnyBusy(int32_t *sender_id, zmq::message_t *zmq_msg);
  static void CommBusRecvAnySleep(int32_t *sender_id, zmq::message_t *zmq_msg);

  static CommBus *comm_bus_;
};
}
