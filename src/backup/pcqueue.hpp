/*
 * utils.hpp
 *
 *  Created on: Aug 15, 2013
 *      Author: jinliang
 */

#ifndef __PETUUM_UTILS_HPP__
#define __PETUUM_UTILS_HPP__

#include <pthread.h>
#include <queue>
#include <semaphore.h>

namespace petuum {
  template<class T>
  class PCQueue{ //producer-consumer queue
  private:
    std::queue<T> q;
    pthread_mutex_t mutex;
    sem_t sem;
  public:
    PCQueue(){
      pthread_mutex_init(&mutex, NULL);
      sem_init(&sem, 0, 0);
    }

    ~PCQueue(){
      pthread_mutex_destroy(&mutex);
      sem_destroy(&sem);
    }

    void Push(T t){
      pthread_mutex_lock(&mutex);
      q.push(t);
      pthread_mutex_unlock(&mutex);
      sem_post(&sem);

    }

    T Pop(){
      sem_wait(&sem); //semaphore lets no more threads in than queue size
      pthread_mutex_lock(&mutex);
      T t = q.front();
      q.pop();
      pthread_mutex_unlock(&mutex);
      return t;
    }
  };
}
#endif /* UTILS_HPP_ */
