#include <petuum_ps/server/ssp_aggr_server_thread.hpp>
#include <petuum_ps/thread/trans_time_estimate.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps_common/util/stats.hpp>

namespace petuum {
void SSPAggrServerThread::SetWaitMsg() {
  WaitMsg_ = WaitMsgTimeOut;
}

long SSPAggrServerThread::ServerIdleWork() {
  if (row_send_milli_sec_ > 1) {
    double send_elapsed_milli = msg_send_timer_.elapsed() * kOneThousand;
    if (row_send_milli_sec_ > send_elapsed_milli + 1)
      return (row_send_milli_sec_ - send_elapsed_milli);
  }

  if (server_obj_.AccumedOpLogSinceLastPush()) {
    size_t sent_bytes
        = server_obj_.CreateSendServerPushRowMsgsPartial(SendServerPushRowMsg);
    row_send_milli_sec_ = TransTimeEstimate::EstimateTransMillisec(sent_bytes);

    msg_send_timer_.restart();

    return row_send_milli_sec_;
  }

  return GlobalContext::get_server_idle_milli();
}

long SSPAggrServerThread::ResetServerIdleMilli() {
  return GlobalContext::get_server_idle_milli();
}

void SSPAggrServerThread::ServerPushRow(bool clock_changed) {
  STATS_SERVER_ACCUM_PUSH_ROW_BEGIN();
  size_t sent_bytes
      = server_obj_.CreateSendServerPushRowMsgs(SendServerPushRowMsg);
  STATS_SERVER_ACCUM_PUSH_ROW_END();

  row_send_milli_sec_ = TransTimeEstimate::EstimateTransMillisec(sent_bytes);
  msg_send_timer_.restart();
}
}
