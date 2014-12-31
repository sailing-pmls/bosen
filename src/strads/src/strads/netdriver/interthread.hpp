// communication method between threads in a process 
// inter-thread queue communication method using dequeue  

#pragma once 

#include <deque>
#include <pthread.h>

template <typename T>
class threadque{

public:
  threadque(void):m_lock(PTHREAD_MUTEX_INITIALIZER){
  }


private:
  std::deque<T>threadq;
  pthread_mutex_t m_lock;


}
