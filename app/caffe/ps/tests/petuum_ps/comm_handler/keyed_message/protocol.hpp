#ifndef __PROTOCOL_HPP__
#define __PROTOCOL_HPP__

#include <pthread.h>

struct IDKey {
  pthread_t thrid_;
  int32_t key_;
};

#endif
