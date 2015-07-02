#include <petuum_ps/server/ssp_push_server_thread.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps_common/thread/mem_transfer.hpp>
#include <petuum_ps_common/util/stats.hpp>

namespace petuum {

void SSPPushServerThread::SendServerPushRowMsg(
    int32_t bg_id, ServerPushRowMsg *msg, bool last_msg,
    int32_t version, int32_t server_min_clock) {
  msg->get_version() = version;
  STATS_SERVER_ADD_PER_CLOCK_PUSH_ROW_SIZE(msg->get_size());
  STATS_SERVER_PUSH_ROW_MSG_SEND_INC_ONE();

  if (last_msg) {
    msg->get_is_clock() = true;
    msg->get_clock() = server_min_clock;
    MemTransfer::TransferMem(GlobalContext::comm_bus, bg_id, msg);
  } else {
    msg->get_is_clock() = false;
    size_t sent_size = (GlobalContext::comm_bus->*(
        GlobalContext::comm_bus->SendAny_))(
        bg_id, msg->get_mem(), msg->get_size());
    CHECK_EQ(sent_size, msg->get_size());
  }
}

void SSPPushServerThread::ServerPushRow(bool clock_changed) {
  STATS_SERVER_ACCUM_PUSH_ROW_BEGIN();
  server_obj_.CreateSendServerPushRowMsgs(SendServerPushRowMsg);
  STATS_SERVER_ACCUM_PUSH_ROW_END();
}

void SSPPushServerThread::RowSubscribe(ServerRow *server_row,
                                       int32_t client_id) {
  server_row->Subscribe(client_id);
}

void SSPPushServerThread::SendOpLogAckMsg(int32_t bg_id, uint32_t version) {
  ServerOpLogAckMsg server_oplog_ack_msg;
  server_oplog_ack_msg.get_ack_version() = version;

  size_t sent_size = (GlobalContext::comm_bus->*(
      GlobalContext::comm_bus->SendAny_))(
          bg_id, server_oplog_ack_msg.get_mem(),
          server_oplog_ack_msg.get_size());
  CHECK_EQ(sent_size, server_oplog_ack_msg.get_size());
}

}
