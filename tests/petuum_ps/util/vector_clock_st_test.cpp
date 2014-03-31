// Copyright (c) 2014, Sailing Lab
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
