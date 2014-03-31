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
#include "petuum_ps/consistency/consistency_controller.hpp"
#include "petuum_ps/storage/dense_row.hpp"
#include "tests/petuum_ps/proxy/dummy_client_proxy.hpp"
#include <gtest/gtest.h>


namespace petuum {

namespace {

const int kProcessStorageCapacity = 3;
const int kThreadCacheCapacity = 3;
const int kMaxPendingOpLogs = 5;

}  // anonymous namespace

class ConsistencyControllerTest : public testing::Test {
  protected:
    ConsistencyControllerTest() {
      // StorageConfig
      StorageConfig storage_config;
      storage_config.capacity = kProcessStorageCapacity;

      // OpLogManagerConfig
      OpLogManagerConfig op_log_config;
      op_log_config.thread_cache_capacity = kThreadCacheCapacity;
      op_log_config.max_pending_op_logs = kMaxPendingOpLogs;

      // ConsistencyControllerConfig.
      controller_config_.table_id = 1;
      controller_config_.storage_config = storage_config;
      controller_config_.policy = &ssp_policy_;
      controller_config_.proxy = &dummy_proxy_;
      controller_config_.op_log_config = op_log_config;

      LOG(INFO) << "ConsistencyControllerTest created.";
    }

    DummyClientProxy<int> dummy_proxy_;

    // policy needs to be alive through out the lifetime of consistency
    // controller. Default is staleness = 0 (BSP).
    SspPolicy<int> ssp_policy_;

    ConsistencyControllerConfig<int> controller_config_;
};

TEST_F(ConsistencyControllerTest, Constructor) {
  ConsistencyController<DenseRow, int> controller(controller_config_);
  ConsistencyController<DenseRow, int> controller2;
  controller2.Init(controller_config_);
}

TEST_F(ConsistencyControllerTest, DoGet) {
  ConsistencyController<DenseRow, int> controller(controller_config_);
  // Dummy client always returns (1, 2, 3) as server row.
  int val;
  EXPECT_EQ(0, controller.DoGet(0, 0, &val));
  EXPECT_EQ(1, val);
}

TEST_F(ConsistencyControllerTest, DoPut) {
  ConsistencyController<DenseRow, int> controller(controller_config_);
  int new_val = 5;
  EXPECT_EQ(0, controller.DoPut(0, 0, new_val));
  int val;
  EXPECT_EQ(0, controller.DoGet(0, 0, &val));
  EXPECT_EQ(5, val);
}

TEST_F(ConsistencyControllerTest, DoInc) {
  ConsistencyController<DenseRow, int> controller(controller_config_);
  // Dummy client always returns (1, 2, 3) as server row.
  int new_val = 5;
  EXPECT_EQ(0, controller.DoInc(0, 0, new_val));
  int val;
  EXPECT_EQ(0, controller.DoGet(0, 0, &val));
  EXPECT_EQ(1+5, val);
}

TEST_F(ConsistencyControllerTest, DoIterate) {
  ConsistencyController<DenseRow, int> controller(controller_config_);
  // Dummy client always returns (1, 2, 3) as server row.
  int new_val = 5;
  EXPECT_EQ(0, controller.DoInc(0, 0, new_val));
  EXPECT_EQ(0, controller.DoIterate());

  // Confirm that op log is cleared.
  int val;
  EXPECT_EQ(0, controller.DoGet(0, 0, &val));
  EXPECT_EQ(1, val);
}

}  // namespace petuum
