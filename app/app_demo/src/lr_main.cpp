#include "lr_app.hpp"
#include <gflags/gflags.h>
#include <glog/logging.h>

DEFINE_int32(num_app_threads, 1, "Number of app threads in this client");

DEFINE_double(lambda, 0.1, "L2 regularization strength.");
DEFINE_int32(batch_size, 100, "mini batch size");
DEFINE_int32(w_staleness, 0, "staleness for w table");

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  LRAppConfig config;
  config.lambda = FLAGS_lambda;
  config.batch_size = FLAGS_batch_size;
  config.w_staleness = FLAGS_w_staleness;
  // TODO(wdai): Set other config parameters from command line flags.
  LRApp lrapp(config);
  lrapp.Run(FLAGS_num_app_threads);
  return 0;
}
