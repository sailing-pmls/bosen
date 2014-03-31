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
#pragma once

#include <glog/logging.h>
#include <boost/unordered_map.hpp>
#include <vector>

namespace petuum {

// VectorClock manages a vector of clocks and maintains the minimum of these
// clocks. This class is single thread (ST) only.
class VectorClock {
public:
  VectorClock();
  // Initialize client_ids.size() client clocks with all of them at time 0.
  explicit VectorClock(const std::vector<int32_t>& ids);
  
  // Add a clock in vector clock with initial timestampe. id must be unique.
  // Return 0 on success, negatives on error (e.g., duplicated id).
  virtual void AddClock(int32_t id, int32_t clock = 0);
  
  // Increment client's clock. Accordingly update slowest_client_clock_. 
  // Return 1 if client_id is the slowest thread; 0 if not; negatives for
  // error;
  virtual int Tick(int32_t id);
  
  // Getters
  virtual int32_t get_clock(int32_t id) const;
  virtual int32_t get_min_clock() const;
  
private:
  // If the tick of this client will change the slowest_client_clock_, then
  // it returns true.
  bool IsUniqueMin(int32_t id);

  boost::unordered_map<int32_t, int32_t> vec_clock_;
  
  // Slowest client clock
  int32_t min_clock_;
};

}  // namespace petuum
