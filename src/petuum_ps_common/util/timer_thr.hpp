#pragma once

#include <stdint.h>
#include <time.h>
#include <zmq.hpp>
#include <pthread.h>

namespace petuum {

class NanoTimer{

  // true if time continue, false if timer stop
  typedef int32_t (*TimerHandler)(void *, int32_t);

  struct TimerThrInfo{
    int32_t interval_;
    TimerHandler callback_;
    void *handler_argu_;
  };

public:
  NanoTimer();

  int Start(int32_t _interval, TimerHandler _handler, void *_handler_argu);

  int Stop();

private:
  static void *TimerThreadMain(void *argu);

  pthread_t thr_;
  TimerThrInfo tinfo_;
  bool started_;

};

}
