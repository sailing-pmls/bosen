#include <glog/logging.h>
#include <petuum_ps_common/util/high_resolution_timer.hpp>
#include <stdint.h>
#include <pthread.h>
#include <gflags/gflags.h>
#include <string.h>

DEFINE_int32(malloc_size, 1024, "Malloc size");
DEFINE_int32(num_threads, 1, "Number of threads");

size_t num_mallocs = 1;

void *ThreadMain(void *mem) {

  uint8_t **addrs = new uint8_t*[num_mallocs];

  petuum::HighResolutionTimer malloc_timer;
  for (int i = 0; i < num_mallocs; ++i) {
    addrs[i] = new uint8_t[FLAGS_malloc_size];
  }

  double malloc_sec = malloc_timer.elapsed();

  petuum::HighResolutionTimer memcpy_timer;
  for (int i = 0; i < num_mallocs; ++i) {
    memcpy(addrs[i], mem, FLAGS_malloc_size);
  }

  double memcpy_sec = memcpy_timer.elapsed();

  petuum::HighResolutionTimer free_timer;
  for (int i = 0; i < num_mallocs; ++i) {
    delete[] addrs[i];
  }

  double free_sec = free_timer.elapsed();

  LOG(INFO) << "num_mallocs = " << num_mallocs
            << " malloc_size = " << FLAGS_malloc_size
            << " malloc_sec = " << malloc_sec
            << " memcpy_sec = " << memcpy_sec
            << " free_sec = " << free_sec
            << " total_sec = " << malloc_sec + free_sec;

  delete[] addrs;
  return 0;
}

int main (int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  LOG(INFO) << "All good";

  num_mallocs = 1854928/FLAGS_num_threads;

  pthread_t *thrs = new pthread_t[FLAGS_num_threads];
  uint8_t **mems = new uint8_t*[FLAGS_num_threads];
  for (int i = 0; i < FLAGS_num_threads; ++i) {
    mems[i] = new uint8_t[FLAGS_malloc_size];
    mems[i][0] = 1;
    pthread_create(thrs + i, 0, ThreadMain, mems[i]);
  }

  for(int i = 0; i < FLAGS_num_threads; ++i) {
    pthread_join(thrs[i], 0);
    delete[] mems[i];
  }

  delete[] thrs;
  delete[] mems;
}
