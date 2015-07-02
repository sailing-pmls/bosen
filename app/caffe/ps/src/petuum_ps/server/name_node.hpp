#pragma once

#include <petuum_ps/server/name_node_thread.hpp>
#include <pthread.h>

namespace petuum {

class NameNode {
public:
  static void Init();
  static void ShutDown();
private:
  static NameNodeThread *name_node_thread_;
  static pthread_barrier_t init_barrier_;
};

}
