// author: jinliang
#pragma once

#include <petuum_ps/server/server_thread_group.hpp>

namespace petuum {

class ServerThreads {
public:
  static void Init(int32_t server_id_st);
  static void ShutDown();

private:
  static ServerThreadGroup *server_thread_group_;
};

}
