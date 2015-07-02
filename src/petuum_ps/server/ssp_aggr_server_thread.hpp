#pragma once

#include <petuum_ps/server/ssp_push_server_thread.hpp>
#include <petuum_ps_common/util/high_resolution_timer.hpp>

namespace petuum {

class SSPAggrServerThread : public SSPPushServerThread {
public:
  SSPAggrServerThread(int32_t my_id, pthread_barrier_t *init_barrier):
      SSPPushServerThread(my_id, init_barrier),
      row_send_milli_sec_(0) { }

  ~SSPAggrServerThread() { }

protected:
  virtual void SetWaitMsg();
  virtual long ServerIdleWork();
  virtual long ResetServerIdleMilli();
  virtual void ServerPushRow(bool clock_changed);

  HighResolutionTimer msg_send_timer_;
  double row_send_milli_sec_;
};

}
