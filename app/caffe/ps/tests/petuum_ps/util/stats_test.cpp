// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.04.07

#include "petuum_ps/util/stats.hpp"
#include <gtest/gtest.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <thread>
#include <chrono>

namespace petuum {

TEST(StatsTest, SmokeTest) {
  google::SetCommandLineOption("stats_table_id", "-1");
  google::SetCommandLineOption("stats_type_id", "1");
  int table_id = 1;
  REGISTER_THREAD_FOR_STATS();
  TIMER_BEGIN(table_id, INC);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  std::this_thread::sleep_for(std::chrono::seconds(1));
  TIMER_END(table_id, INC);
  FINALIZE_STATS();
  PRINT_STATS();
}

}  // namespace petuum
