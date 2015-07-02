#pragma once

#include <petuum_ps/server/ssp_push_server_thread.hpp>
#include <petuum_ps_common/util/high_resolution_timer.hpp>

namespace petuum {

class SSPAggrServerThread : public SSPPushServerThread {
public:
  SSPAggrServerThread(int32_t my_id, pthread_barrier_t *init_barrier):
      SSPPushServerThread(my_id, init_barrier),
      row_send_milli_sec_(0),
      early_comm_on_(false),
      num_early_comm_off_msgs_(0) {
    ResetServerIdleMilli_ = &SSPAggrServerThread::ResetServerIdleMilliNoEarlyComm;
  }

  ~SSPAggrServerThread() { }

protected:
  virtual void SetWaitMsg();
  virtual long ServerIdleWork();
  virtual long ResetServerIdleMilli();
  virtual void ServerPushRow();

  void HandleEarlyCommOn();
  void HandleEarlyCommOff();

  long ResetServerIdleMilliNoEarlyComm();
  long ResetServerIdleMilliEarlyComm();

  void PrepareBeforeInfiniteLoop();
  void ClockNotice();
  void AdjustSuppressionLevel(int32_t bg_id, int32_t bg_clock);

  typedef long (SSPAggrServerThread::*ResetServerIdleMilliFunc)();

  HighResolutionTimer msg_send_timer_;
  double row_send_milli_sec_;
  ResetServerIdleMilliFunc ResetServerIdleMilli_;
  bool early_comm_on_;
  size_t num_early_comm_off_msgs_;
};

}
