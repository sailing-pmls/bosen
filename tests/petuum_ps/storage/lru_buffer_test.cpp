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
#include "petuum_ps/storage/lru_buffer.hpp"
#include "petuum_ps/storage/lru_eviction_logic.hpp"
#include <gtest/gtest.h>
#include <boost/thread.hpp>

namespace petuum
{

  TEST(LruBufferTest, Constructors)
  {

    int num_buffer = 2;
    int drain_thresh = 5;
    MultiBuffer buffers(num_buffer, drain_thresh);

    EvictionLogic::list_iter_t* list = new EvictionLogic::list_iter_t [100];

    for (int i = 0; i < 100; i++)
    {
      EvictionLogic::list_iter_t it = buffers.put(i);
      list[i] = it;
    }


    for (int i = 1; i < 100; i++)
    {
      buffers.add_to_buffer(list[i]);
    }

    buffers.drain_buffer();

  };

  TEST(LruBufferTest, test_add_and_drain)
  {

    int num_buffer = 2;
    int drain_thresh = 5;
    MultiBuffer buffers(num_buffer, drain_thresh);

    EvictionLogic::list_iter_t* list = new EvictionLogic::list_iter_t [100];

    for (int i = 0; i < 10; i++)
    {
      EvictionLogic::list_iter_t it = buffers.put(i);
      list[i] = it;
    }

    for (int i = 1; i < 10; i++)
    {
      buffers.add_to_buffer(list[i]);
    }

    EXPECT_EQ(buffers.evict_drain_buffer(),0);
    EXPECT_EQ(buffers.evict_drain_buffer(),1);
    for (int i = 0; i < 100; i++)
    {
      buffers.add_to_buffer(list[2]);
    }
    EXPECT_EQ(buffers.evict_drain_buffer(),3);
  };



  void touching(MultiBuffer* bufs, EvictionLogic::list_iter_t* list, int id)
  {
    for (int i = 0; i < 100; i++)
    {
      bufs->add_to_buffer(list[id]);
    }
  }

  TEST(LruBufferTest, concurrent_test)
  {

    int num_buffer = 5;
    int drain_thresh = 5;
    MultiBuffer buffers(num_buffer, drain_thresh);

    EvictionLogic::list_iter_t* list = new EvictionLogic::list_iter_t [100];

    for (int i = 0; i < 100; i++)
    {
      EvictionLogic::list_iter_t it = buffers.put(i);
      list[i] = it;
    }

    boost::thread* workers[100];

    for (int i = 0; i < 99; i++)
    {
      workers[i] = new boost::thread(touching, &buffers, list, i);
    }

    for (int i = 0; i < 99; i++)
    {
      workers[i]->join();
    }

    EXPECT_EQ(buffers.evict_drain_buffer(),99);
  };
}
