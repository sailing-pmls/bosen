// Copyright (c) 2014, Sailing Lab
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
#include <petuum_ps_common/util/timer_thr.hpp>

#include <sys/time.h>
#include <glog/logging.h>

namespace petuum {

NanoTimer::NanoTimer():
  started_(false){}

int NanoTimer::Start(int32_t _interval, TimerHandler _handler,
		     void *_handler_argu){

  if(started_) return -1;
  if(_interval <= 0) return -1;

  tinfo_.interval_ = _interval;
  tinfo_.callback_ = _handler;
  tinfo_.handler_argu_ = _handler_argu;

  int ret = pthread_create(&thr_, NULL, TimerThreadMain, &tinfo_);
  if(ret != 0) return -1;
  started_ = true;

  return 0;
}

int NanoTimer::Stop(){
  if(!started_) return -1;
  pthread_join(thr_, NULL);
  started_ = false;
  return 0;
}

void *NanoTimer::TimerThreadMain(void *argu){
  TimerThrInfo *tinfo = reinterpret_cast<TimerThrInfo*>(argu);
  int32_t interval = tinfo->interval_;
  while(1){
    timespec req;
    req.tv_sec = 0;
    req.tv_nsec = interval;
    timespec rem;

    nanosleep(&req, &rem);

    interval = tinfo->callback_(tinfo->handler_argu_, rem.tv_nsec);
    if(interval <= 0) break;
  }
  return 0;
}

}
