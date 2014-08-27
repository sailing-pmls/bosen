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
#include <petuum_ps_common/util/vector_clock.hpp>

namespace petuum {

VectorClock::VectorClock():
  min_clock_(-1){}

VectorClock::VectorClock(const std::vector<int32_t>& ids):
  min_clock_(0){
  int size = ids.size();
  for (int i = 0; i < size; ++i) {
    int32_t id = ids[i];

    // Initialize client clock with zeros.
    vec_clock_[id] = 0;
  }
}

void VectorClock::AddClock(int32_t id, int32_t clock)
{
  vec_clock_[id] = clock;
  // update slowest
  if (min_clock_ == -1 || clock < min_clock_) {
    min_clock_ = clock;
  }
}

int32_t VectorClock::Tick(int32_t id)
{
  if (IsUniqueMin(id)) {
    ++vec_clock_[id];
    return ++min_clock_;
  }
  ++vec_clock_[id];
  return 0;
}

int32_t VectorClock::get_clock(int32_t id) const
{
  auto iter = vec_clock_.find(id);
  CHECK(iter != vec_clock_.end());
  return iter->second;
}

int32_t VectorClock::get_min_clock() const
{
  return min_clock_;
}

// =========== Private Functions ============

bool VectorClock::IsUniqueMin(int32_t id)
{
  if (vec_clock_[id] != min_clock_) {
    // definitely not the slowest.
    return false;
  }
  // check if it is also unique
  int num_min = 0;
  for (auto iter = vec_clock_.cbegin(); iter != vec_clock_.cend(); iter++) {
    if (iter->second == min_clock_) ++num_min;
    if (num_min > 1) return false;
  }
  return true;
}

}  // namespace petuum
