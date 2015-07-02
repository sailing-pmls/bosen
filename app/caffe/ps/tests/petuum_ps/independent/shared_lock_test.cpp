
#include <iostream>
#include <pthread.h>
#include <cassert>
#include <stdint.h>
#include <stddef.h>
#include <boost/thread.hpp>   // pthread status code
#include <glog/logging.h>

#include <petuum_ps_common/util/high_resolution_timer.hpp>
#include <petuum_ps_common/util/lock.hpp>

#define NUM_THREADS 64

petuum::SharedMutex smtx;
int32_t global_cnt = 0;
size_t loop_cnt = 1000;
pthread_t thr[NUM_THREADS];


void *ThreadMain(void *info) {
  petuum::HighResolutionTimer timer;
  for (size_t i = 0; i < loop_cnt; ++i) {
    smtx.lock_shared();
    global_cnt += 1;
    smtx.unlock_shared();
  }
  double elapsed_sec = timer.elapsed();

  LOG(INFO) << "elapsed_sec = " << elapsed_sec << " sec";
  return 0;
}


int main(int argc, char *argv[]) {
  google::InitGoogleLogging(argv[0]);

  for (size_t i = 0; i < NUM_THREADS; ++i) {
    pthread_create (thr + i, NULL, ThreadMain, NULL);
  }

  for (size_t i = 0; i < NUM_THREADS; ++i) {
    pthread_join (thr[i], NULL);
  }
  return 0;
}
