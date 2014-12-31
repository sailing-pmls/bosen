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
