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
#include "petuum_ps/storage/lru_eviction_logic.hpp"
#include <gtest/gtest.h>

namespace petuum
{

TEST(LruEvictionLogicTest, Constructors)
{
  int cache_size = 1000;
  // hard code the expected results
  int expected_active_list_capacity = static_cast<int>(1000.0*0.95);
  // test the other constructor
  int active_list_capacity = 100;
  int threshold_promote_to_active = 2;
  EvictionLogic eviction_logic(active_list_capacity,
      threshold_promote_to_active);
  EXPECT_EQ(eviction_logic.get_active_list_capacity(),
      active_list_capacity);
  EXPECT_EQ(eviction_logic.get_threshold(),
      threshold_promote_to_active);
};

TEST(LruEvictionLogicTest, TestEvict)
{
  int active_list_capacity = 1;
  int threshold_promote_to_active = 1;
  EvictionLogic eviction_logic(active_list_capacity,
      threshold_promote_to_active);
  int32_t row_id0 = 0;
  int32_t row_id1 = 1;
  int32_t row_id2 = 2;
  int32_t row_id3 = 3;
  int32_t row_id4 = 4;
  EvictionLogic::list_iter_t row0_it =
    eviction_logic.Put(row_id0);
  int32_t tobe_evicted = eviction_logic.Get_evict();
  // there is only 1 element inside the logic,
  // has to evict that element
  EXPECT_EQ(tobe_evicted,row_id0);
  eviction_logic.Put(row_id1);
  eviction_logic.Put(row_id2);
  eviction_logic.Put(row_id3);
  eviction_logic.Put(row_id4);
  tobe_evicted = eviction_logic.Get_evict();
  EXPECT_EQ(tobe_evicted,row_id0);
  eviction_logic.Erase(row0_it);
  tobe_evicted = eviction_logic.Get_evict();
  EXPECT_EQ(tobe_evicted,1);
};


TEST(LruEvictionLogicTest, TestTouch)
{
  int active_list_capacity = 1;
  int threshold_promote_to_active = 1;
  EvictionLogic eviction_logic(active_list_capacity,
      threshold_promote_to_active);
  int32_t row_id0 = 0;
  int32_t row_id1 = 1;
  int32_t row_id2 = 2;
  int32_t row_id3 = 3;
  int32_t row_id4 = 4;
  EvictionLogic::list_iter_t row0_it =
    eviction_logic.Put(row_id0);
  eviction_logic.Put(row_id1);
  eviction_logic.Put(row_id2);
  eviction_logic.Put(row_id3);
  eviction_logic.Put(row_id4);
  int32_t tobe_evicted = eviction_logic.Get_evict();
  EXPECT_EQ(tobe_evicted,0);
  eviction_logic.Touch(row0_it);
  eviction_logic.Touch(row0_it);
  eviction_logic.Touch(row0_it);
  eviction_logic.Touch(row0_it);
  eviction_logic.Touch(row0_it);
  tobe_evicted = eviction_logic.Get_evict();
  EXPECT_EQ(tobe_evicted,1);
}


TEST(LruEvictionLogicTest, TestReadCount)
{
  int active_list_capacity = 100;
  int threshold_promote_to_active = 100;
  EvictionLogic eviction_logic(active_list_capacity,
      threshold_promote_to_active);
  int32_t row_id0 = 0;
  EvictionLogic::list_iter_t row0_it =
    eviction_logic.Put(row_id0);
  int32_t row_id1 = 1;
  EvictionLogic::list_iter_t row1_it =
    eviction_logic.Put(row_id1);
  for (int i = 0; i < 99; i++)
  {
    EXPECT_EQ(boost::get<1>(*row0_it),i);
    eviction_logic.Touch(row0_it);
  }
  for (int i = 0; i < 99; i++)
  {
    EXPECT_EQ(boost::get<1>(*row1_it),i);
    eviction_logic.Touch(row1_it);
  }
}



TEST(LruEvictionLogicTest, TestActiveList)
{
  int active_list_capacity = 1;
  int threshold_promote_to_active = 100;
  EvictionLogic eviction_logic(active_list_capacity,
      threshold_promote_to_active);
  int32_t row_id0 = 0;
  EvictionLogic::list_iter_t row0_it =
    eviction_logic.Put(row_id0);
  int32_t row_id1 = 1;
  EvictionLogic::list_iter_t row1_it =
    eviction_logic.Put(row_id1);
  eviction_logic.Put(2);
  eviction_logic.Put(3);
  eviction_logic.Put(4);
  eviction_logic.Put(5);
  eviction_logic.Put(6);
  // promote row 0
  for (int i = 0; i < 100; i++)
  {
    eviction_logic.Touch(row0_it);
  }
  // row 0 read counter should go up to 200
  for (int i = 0; i < 100; i++)
  {
    eviction_logic.Touch(row0_it);
  }
  // promote row 1
  // row 0 should be migrate back to inactive list
  // and its read count should be reset to 0
  for (int i = 0; i < 100; i++)
  {
    eviction_logic.Touch(row1_it);
  }
  EXPECT_EQ(boost::get<1>(*row0_it),0);
}

TEST(LruEvictionLogicTest, TestActiveListCapacity)
{
  int active_list_capacity = 1;
  int threshold_promote_to_active = 1;
  EvictionLogic eviction_logic(active_list_capacity,
      threshold_promote_to_active);
  int32_t row_id0 = 0;
  int32_t row_id1 = 1;
  EvictionLogic::list_iter_t row0_it =
    eviction_logic.Put(row_id0);
  EvictionLogic::list_iter_t row1_it =
    eviction_logic.Put(row_id1);
  eviction_logic.Put(2);
  eviction_logic.Put(3);
  eviction_logic.Put(4);
  eviction_logic.Put(5);
  eviction_logic.Put(6);
  eviction_logic.Touch(row0_it);
  eviction_logic.Touch(row1_it);
  int32_t tobe_evicted = eviction_logic.Get_evict();
  EXPECT_EQ(tobe_evicted,2);
  eviction_logic.Touch(row0_it);
  tobe_evicted = eviction_logic.Get_evict();
  EXPECT_EQ(tobe_evicted,2);
}

TEST(LruEvictionLogicTest, TestEmptyInactiveList)
{
  int active_list_capacity = 10;
  int threshold_promote_to_active = 1;
  EvictionLogic eviction_logic(active_list_capacity,
      threshold_promote_to_active);
  int32_t row_id0 = 0;
  int32_t row_id1 = 1;
  EvictionLogic::list_iter_t row0_it =
    eviction_logic.Put(row_id0);
  EvictionLogic::list_iter_t row1_it =
    eviction_logic.Put(row_id1);
  eviction_logic.Touch(row0_it);
  eviction_logic.Touch(row1_it);
  eviction_logic.Touch(row1_it);
  eviction_logic.Touch(row1_it);
  eviction_logic.Touch(row1_it);
  eviction_logic.Touch(row1_it);
  eviction_logic.Touch(row1_it);
  eviction_logic.Touch(row1_it);
  int32_t tobe_evicted = eviction_logic.Get_evict();
  EXPECT_EQ(tobe_evicted,1);
}


}// petuum namespace
