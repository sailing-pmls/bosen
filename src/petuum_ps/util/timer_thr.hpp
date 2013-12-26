// Copyright (c) 2013, SailingLab
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the <ORGANIZATION> nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef __PETUUM_TIMER_THR_HPP__
#define __PETUUM_TIMER_THR_HPP__

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

#endif
