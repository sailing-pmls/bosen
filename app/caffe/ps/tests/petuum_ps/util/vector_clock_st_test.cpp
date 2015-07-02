// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2013.12.25

#include "petuum_ps/util/vector_clock_st.hpp"
#include <vector>
#include <gtest/gtest.h>

namespace petuum {

class VectorClockSTTest : public testing::Test {
  protected:
    VectorClockSTTest() {
        // Add three clients
      EXPECT_EQ(0, vclock_.AddClock(0));
      EXPECT_EQ(0, vclock_.AddClock(1));
      EXPECT_EQ(0, vclock_.AddClock(2));
    }

    VectorClockST vclock_;
};

TEST_F(VectorClockSTTest, Constructor) {
  std::vector<int> clients(3);
  clients[0] = 0;
  clients[1] = 1;
  clients[2] = 2;
  VectorClockST vclock(clients);

  // Check all clock are 0.
  EXPECT_EQ(0, vclock.get_client_clock(0));
  EXPECT_EQ(0, vclock.get_client_clock(1));
  EXPECT_EQ(0, vclock.get_client_clock(2));
  EXPECT_EQ(0, vclock.get_slowest_clock());
}

TEST_F(VectorClockSTTest, ClockInit) {
  // Check all clock are 0.
  EXPECT_EQ(0, vclock_.get_client_clock(0));
  EXPECT_EQ(0, vclock_.get_client_clock(1));
  EXPECT_EQ(0, vclock_.get_client_clock(2));
  EXPECT_EQ(0, vclock_.get_slowest_clock());

  // Check assignment constructor.
  VectorClockST vclock2 = vclock_;
  EXPECT_EQ(0, vclock2.get_client_clock(0));
  EXPECT_EQ(0, vclock2.get_client_clock(1));
  EXPECT_EQ(0, vclock2.get_client_clock(2));
  EXPECT_EQ(0, vclock2.get_slowest_clock());
}

TEST_F(VectorClockSTTest, ClockTick) {
  vclock_.Tick(0);
  vclock_.Tick(0);
  EXPECT_EQ(2, vclock_.get_client_clock(0));
  EXPECT_EQ(0, vclock_.get_slowest_clock());

  vclock_.Tick(1);
  EXPECT_EQ(1, vclock_.get_client_clock(1));
  EXPECT_EQ(0, vclock_.get_slowest_clock());

  vclock_.Tick(2);
  EXPECT_EQ(1, vclock_.get_client_clock(2));
  EXPECT_EQ(1, vclock_.get_slowest_clock());

  VectorClockST vclock2 = vclock_;
  EXPECT_EQ(2, vclock2.get_client_clock(0));
  EXPECT_EQ(1, vclock2.get_client_clock(1));
  EXPECT_EQ(1, vclock2.get_client_clock(2));
  EXPECT_EQ(1, vclock2.get_slowest_clock());
}

}  // namespace petuum
