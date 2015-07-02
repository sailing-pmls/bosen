#include <petuum_ps/server/ssp_aggr_server_thread.hpp>
#include <petuum_ps/thread/trans_time_estimate.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps_common/util/stats.hpp>
#include <petuum_ps/thread/ps_msgs.hpp>
#include <algorithm>

namespace petuum {
void SSPAggrServerThread::SetWaitMsg() {
  WaitMsg_ = WaitMsgTimeOut;
}

long SSPAggrServerThread::ServerIdleWork() {
  //LOG(INFO) << __func__;
  if (row_send_milli_sec_ > 1) {
    double send_elapsed_milli = msg_send_timer_.elapsed() * kOneThousand;
    //LOG(INFO) << "send_elapsed_milli = " << send_elapsed_milli
    //      << " row_send_milli_sec_ = " << row_send_milli_sec_;
    if (row_send_milli_sec_ > send_elapsed_milli + 1)
      return (row_send_milli_sec_ - send_elapsed_milli);
  }

  if (!msg_tracker_.CheckSendAll()) {
    STATS_SERVER_ACCUM_WAITS_ON_ACK_IDLE();
    return GlobalContext::get_server_idle_milli();
  }

  STATS_SERVER_IDLE_INVOKE_INC_ONE();

  //STATS_SERVER_ACCUM_IDLE_SEND_BEGIN();
  size_t sent_bytes
      = server_obj_.CreateSendServerPushRowMsgsPartial(SendServerPushRowMsg);

  //STATS_SERVER_ACCUM_IDLE_SEND_END();

  if (sent_bytes > 0) {
      STATS_SERVER_IDLE_SEND_INC_ONE();
      STATS_SERVER_ACCUM_IDLE_ROW_SENT_BYTES(sent_bytes);

      row_send_milli_sec_ = TransTimeEstimate::EstimateTransMillisec(
          sent_bytes, GlobalContext::get_server_bandwidth_mbps());

      //LOG(INFO) << "ServerIdle send bytes = " << sent_bytes
      //        << " bw = " << GlobalContext::get_bandwidth_mbps()
      //        << " send milli sec = " << row_send_milli_sec_
      //        << " server_id = " << my_id_;

      msg_send_timer_.restart();

      return row_send_milli_sec_;
  }
  //LOG(INFO) << "Server nothing to send";

  return GlobalContext::get_server_idle_milli();
}

long SSPAggrServerThread::ResetServerIdleMilli() {
  return (this->*ResetServerIdleMilli_)();
}

long SSPAggrServerThread::ResetServerIdleMilliNoEarlyComm() {
  return -1;
}

long SSPAggrServerThread::ResetServerIdleMilliEarlyComm() {
  return GlobalContext::get_server_idle_milli();
}

void SSPAggrServerThread::ServerPushRow() {
  //LOG(INFO) << __func__;
  if (!msg_tracker_.CheckSendAll()) {
    //LOG(INFO) << "server has to wait";
    STATS_SERVER_ACCUM_WAITS_ON_ACK_CLOCK();
    pending_clock_push_row_ = true;
    return;
  }

  STATS_SERVER_ACCUM_PUSH_ROW_BEGIN();
  size_t sent_bytes
      = server_obj_.CreateSendServerPushRowMsgs(SendServerPushRowMsg);
  STATS_SERVER_ACCUM_PUSH_ROW_END();

  double left_over_send_milli_sec = 0;

  if (row_send_milli_sec_ > 1) {
    double send_elapsed_milli = msg_send_timer_.elapsed() * kOneThousand;
    left_over_send_milli_sec = std::max<double>(0, row_send_milli_sec_ - send_elapsed_milli);
  }

  row_send_milli_sec_ = TransTimeEstimate::EstimateTransMillisec(
      sent_bytes, GlobalContext::get_server_bandwidth_mbps())
                        + left_over_send_milli_sec;
  msg_send_timer_.restart();

  //LOG(INFO) << "Server clock sent_size = " << sent_bytes
  //        << " row_send_milli_sec = " << row_send_milli_sec_
  //        << " left_over_send_milli_sec = " << left_over_send_milli_sec;
}

void SSPAggrServerThread::HandleEarlyCommOn() {
  if (!early_comm_on_) {
    ResetServerIdleMilli_ = &SSPAggrServerThread::ResetServerIdleMilliEarlyComm;
    early_comm_on_ = true;
    num_early_comm_off_msgs_ = 0;
  }
  //LOG(INFO) << "Turn on early comm";
}

void SSPAggrServerThread::HandleEarlyCommOff() {
  num_early_comm_off_msgs_++;
  if (num_early_comm_off_msgs_ == bg_worker_ids_.size()) {
    ResetServerIdleMilli_ = &SSPAggrServerThread::ResetServerIdleMilliNoEarlyComm;
    early_comm_on_ = false;
    num_early_comm_off_msgs_ = 0;
  }
}

void SSPAggrServerThread::PrepareBeforeInfiniteLoop() { }


void SSPAggrServerThread::ClockNotice() { }

void SSPAggrServerThread::AdjustSuppressionLevel(
    int32_t bg_id, int32_t bg_clock) {
  bool changed = client_progress_clock_.TickUntil(bg_id, bg_clock);
  if (!changed) return;

  if (client_progress_clock_.IsUniqueMax(bg_id)) {
    int32_t min_clock = client_progress_clock_.get_min_clock();
    if (bg_clock - min_clock >= 2) {
      AdjustSuppressionLevelMsg msg;
      for (auto &id : bg_worker_ids_) {
        int32_t clock = client_progress_clock_.get_clock(id);
        if (clock == min_clock) {
          size_t sent_size = (GlobalContext::comm_bus->*(
              GlobalContext::comm_bus->SendAny_))(
                  id, msg.get_mem(), msg.get_size());
          CHECK_EQ(sent_size, msg.get_size());
        }
      }
    }
  }
}

}
