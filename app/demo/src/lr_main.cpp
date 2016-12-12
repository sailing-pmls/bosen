#include "lr_app.hpp"
#include <gflags/gflags.h>
#include <glog/logging.h>

DEFINE_string(input_dir, "", "Data location");
DEFINE_double(learning_rate, 1.0, "Learning rate.");
DEFINE_double(lambda, 1000, "L2 regularization strength.");
DEFINE_int32(batch_size, 100, "mini batch size");
DEFINE_int32(w_staleness, 0, "staleness for w table");

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  LRAppConfig config;
  config.input_dir = FLAGS_input_dir;
  config.learning_rate = FLAGS_learning_rate;
  config.lambda = FLAGS_lambda;
  config.batch_size = FLAGS_batch_size;
  config.w_staleness = FLAGS_w_staleness;
  
  LRApp lrapp(config);
  lrapp.Run(FLAGS_num_app_threads);
  return 0;
}
