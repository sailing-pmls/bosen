// author: jinliang
#pragma once

#include <petuum_ps/server/server_thread.hpp>

namespace petuum {
class ServerThreadGroup {
public:
  ServerThreadGroup(int32_t server_id_st);
  ~ServerThreadGroup();

  void Start();
  void ShutDown();
private:
  std::vector<ServerThread*> server_thread_vec_;
  pthread_barrier_t init_barrier_;
};

}
