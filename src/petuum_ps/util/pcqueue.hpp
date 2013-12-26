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
