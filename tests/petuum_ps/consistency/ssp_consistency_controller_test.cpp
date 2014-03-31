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

#include "petuum_ps/consistency/ssp_consistency_controller.hpp"
#include "petuum_ps/include/configs.hpp"
#include "petuum_ps/storage/sparse_row.hpp"
#include "petuum_ps/util/vector_clock.hpp"
#include <gtest/gtest.h>

namespace petuum {

namespace {

const int kProcessStorageCapacity = 10;
const int kTableID = 1;
const int kOpLogPartitionCapacity = 10;
const int kUpdateSize = sizeof(int);
const int kNumThreads = 1;

}  // anonymous namespace

class SSPConsistencyControllerTest : public testing::Test {
protected:
  SSPConsistencyControllerTest() : 
    process_storage_(kProcessStorageCapacity),
    oplog_(kTableID, kOpLogPartitionCapacity, kUpdateSize, &sample_row_),
    vector_clock_(kNumThreads), controller_(table_info_, &process_storage_,
        &oplog_, &vector_clock_, &sample_row_) {
      table_info_.table_staleness = 0;
    }

  TableInfo table_info_;

  ProcessStorage process_storage_;

  SparseRow<int> sample_row_;

  TableOpLog oplog_;

  VectorClock vector_clock_;

  SSPConsistencyController controller_;
};

TEST_F(SSPConsistencyControllerTest, Constructor) {
}

}  // namespace petuum
