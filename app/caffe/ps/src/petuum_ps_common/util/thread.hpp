#pragma once

#include <pthread.h>

namespace petuum {
class Thread {
public:
  Thread() { }

  virtual ~Thread() { }

  virtual void *operator() () {
    return 0;
  }

  int Start() {
    InitWhenStart();
    return pthread_create(&thr_, NULL, InternalStart, this);
  }

  void Join() {
    pthread_join(thr_, NULL);
  }

protected:
  virtual void InitWhenStart() { }

private:
  static void *InternalStart(void *thread) {
    Thread *t = reinterpret_cast<Thread*>(thread);
    return t->operator()();
  }

  pthread_t thr_;
};
}
