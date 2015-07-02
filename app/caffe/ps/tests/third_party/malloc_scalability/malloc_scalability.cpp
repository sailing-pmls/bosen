#include <glog/logging.h>
#include <petuum_ps_common/util/high_resolution_timer.hpp>
#include <stdint.h>
#include <pthread.h>
#include <gflags/gflags.h>

DEFINE_int32(malloc_size, 1024, "Malloc size");

const size_t kNumMallocs = 1854928;

void *ThreadMain(void *info) {

  uint8_t *addrs[kNumMallocs];

  petuum::HighResolutionTimer malloc_timer;
  for (int i = 0; i < kNumMallocs; ++i) {
    addrs[i] = new uint8_t[FLAGS_malloc_size];
  }

  double malloc_sec = malloc_timer.elapsed();

  petuum::HighResolutionTimer free_timer;
  for (int i = 0; i < kNumMallocs; ++i) {
    delete[] addrs[i];
  }

  LOG(INFO) << "Malloc sec elapsed = " << malloc_sec
            << " Free sec elapsed = " << free_timer.elapsed();
}

DEFINE_int32(num_threads, 1, "Number of threads");

int main (int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  LOG(INFO) << "All good";

  pthread_t *thrs = new pthread_t[FLAGS_num_threads];
  for (int i = 0; i < FLAGS_num_threads; ++i) {
    pthread_create(thrs + i, 0, ThreadMain, 0);
  }

  for(int i = 0; i < FLAGS_num_threads; ++i) {
    pthread_join(thrs[i], 0);
  }

  delete[] thrs;
}
