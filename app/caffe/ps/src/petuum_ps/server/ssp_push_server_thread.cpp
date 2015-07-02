#include <petuum_ps/server/ssp_push_server_thread.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps_common/thread/mem_transfer.hpp>
#include <petuum_ps_common/util/stats.hpp>

namespace petuum {

void SSPPushServerThread::SendServerPushRowMsg(
    int32_t bg_id, ServerPushRowMsg *msg, bool last_msg,
    int32_t version, int32_t server_min_clock,
    MsgTracker *msg_tracker) {

  //if (last_msg)
  //LOG(INFO) << "server_send_push_row, is_clock = " << last_msg
  //        << " clock = " << server_min_clock
  //	    << " to = " << bg_id
  //        << " " << ThreadContext::get_id();

  msg->get_version() = version;
  msg->get_seq_num() = msg_tracker->IncGetSeq(bg_id);
  STATS_SERVER_ADD_PER_CLOCK_PUSH_ROW_SIZE(msg->get_size());
  STATS_SERVER_PUSH_ROW_MSG_SEND_INC_ONE();

  //LOG(INFO) << "send " << bg_id << " " << msg->get_seq_num();

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

void SSPPushServerThread::ServerPushRow() {
  if (!msg_tracker_.CheckSendAll()) {
    STATS_SERVER_ACCUM_WAITS_ON_ACK_CLOCK();
    pending_clock_push_row_ = true;
    return;
  }
  //LOG(INFO) << __func__;
  STATS_SERVER_ACCUM_PUSH_ROW_BEGIN();
  server_obj_.CreateSendServerPushRowMsgs(SendServerPushRowMsg);
  STATS_SERVER_ACCUM_PUSH_ROW_END();
}

void SSPPushServerThread::RowSubscribe(ServerRow *server_row,
                                       int32_t client_id) {
  server_row->Subscribe(client_id);
}

void SSPPushServerThread::HandleBgServerPushRowAck(
    int32_t bg_id, uint64_t ack_seq) {
  //LOG(INFO) << "acked " << bg_id << " " << ack_seq;
  msg_tracker_.RecvAck(bg_id, ack_seq);
  if (pending_clock_push_row_
      && msg_tracker_.CheckSendAll()) {
    pending_clock_push_row_ = false;
    ServerPushRow();
  }
}

}
