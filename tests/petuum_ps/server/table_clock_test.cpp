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

#include "petuum_ps/server/table_clock.hpp"

#include <vector>
#include <gtest/gtest.h>
#include <boost/scoped_ptr.hpp>

namespace {

const int num_clients = 3;
const int num_ticks = 1000;
const int variation = 20;

}

namespace petuum {

class TableClockTest : public testing::Test {
  protected:
    TableClockTest() {
      ids.resize(num_clients);
      for (int i = 0; i < num_clients; ++i) {
        ids[i] = i + 100;
      }
      clock_.reset(new TableClock(ids, (int)num_clients));
    } // end of ctor

    void FairTick(int ticks) {
      for (int i = 0; i < ticks; ++i) {
        clock_->Tick(101);
        clock_->Tick(102);
        clock_->Tick(100);
      }
    }

    void UnFairTick(int ticks, int var) {
      FairTick(ticks);
      for (int i = 0; i < var; ++i) {
        clock_->Tick(101);
      }
      for (int i = 0; i < var-1; ++i) {
        clock_->Tick(102);
      }
    }

    boost::scoped_ptr<TableClock> clock_;
    std::vector<int32_t> ids;
};

TEST_F(TableClockTest, Constructor) {
  EXPECT_EQ(0, clock_->get_server_clock());
  EXPECT_EQ(0, clock_->get_client_clock(100));
  EXPECT_EQ(0, clock_->get_client_clock(101));
  EXPECT_EQ(0, clock_->get_client_clock(102));
}

TEST_F(TableClockTest, FairTick) {
  FairTick(num_ticks);
  EXPECT_EQ(num_ticks, clock_->get_server_clock());
  EXPECT_EQ(num_ticks, clock_->get_client_clock(100));
  EXPECT_EQ(num_ticks, clock_->get_client_clock(101));
  EXPECT_EQ(num_ticks, clock_->get_client_clock(102));
  EXPECT_EQ(0, clock_->DistanceFromSlowest(100));
  EXPECT_EQ(0, clock_->DistanceFromSlowest(101));
  EXPECT_EQ(0, clock_->DistanceFromSlowest(102));
}

TEST_F(TableClockTest, UnFairTick) {
  UnFairTick(num_ticks, variation);
  EXPECT_EQ(num_ticks, clock_->get_server_clock());
  EXPECT_EQ(num_ticks, clock_->get_client_clock(100));
  EXPECT_EQ(num_ticks + variation, clock_->get_client_clock(101));
  EXPECT_EQ(num_ticks + variation - 1, clock_->get_client_clock(102));
  EXPECT_EQ(0, clock_->DistanceFromSlowest(100));
  EXPECT_EQ(variation, clock_->DistanceFromSlowest(101));
  EXPECT_EQ(variation - 1, clock_->DistanceFromSlowest(102));
}


}  // namespace petuum
