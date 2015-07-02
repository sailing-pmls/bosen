#pragma once

#include <petuum_ps/server/server_thread.hpp>

namespace petuum {

class SSPPushServerThread : public ServerThread {
public:
  SSPPushServerThread(int32_t my_id, pthread_barrier_t *init_barrier):
      ServerThread(my_id, init_barrier) { }

  ~SSPPushServerThread() { }
protected:
  virtual void ServerPushRow();
  static void SendServerPushRowMsg (int32_t bg_id, ServerPushRowMsg *msg,
                                    bool last_msg, int32_t version,
                                    int32_t server_min_clock,
                                    MsgTracker *msg_tracker);

  virtual void RowSubscribe(ServerRow *server_row, int32_t client_id);

  void HandleBgServerPushRowAck(
      int32_t bg_id, uint64_t ack_seq);

};

}
