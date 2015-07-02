// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.01.27

#include <gtest/gtest.h>
#include <glog/logging.h>
#include <libcuckoo/cuckoohash_map.hh>

namespace petuum {

TEST(CuckooTest, SmokeTest) {
  VLOG(0) << "begin smoke test";
  cuckoohash_map<int, int> cuckoo_map;
  EXPECT_TRUE(cuckoo_map.insert(0, 1));
  int val;
  EXPECT_TRUE(cuckoo_map.find(0, val));
}

}  // namespace petuum
