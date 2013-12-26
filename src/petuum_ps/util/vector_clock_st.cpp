// Copyright (c) 2013, SailingLab
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

#include "petuum_ps/util/vector_clock_st.hpp"

namespace petuum {

VectorClockST::VectorClockST() : slowest_client_clock_(-1) {}

VectorClockST::VectorClockST(const std::vector<int32_t>& client_ids) {
  int num_clients = client_ids.size();
  for (int i = 0; i < num_clients; ++i) {
    VLOG(1) << "VectorClockST client_ids[" << i << "] = " << client_ids[i];
    int32_t client_id = client_ids[i];

    // Initialize client clock with zeros.
    vec_clock_[client_id] = 0;
    VLOG(1) << "client_id = " << client_id << " added!";
  }
  slowest_client_clock_ = 0;
}

int VectorClockST::AddClock(int32_t client_id, int32_t timestamp)
{
  vec_clock_t::iterator it = vec_clock_.find(client_id);
  if (it != vec_clock_.end()) {
    // already there. return error
    LOG(ERROR) << "key " << client_id << " is already in the VectorClockST";
    return -1;
  }
  // not found. create one.
  vec_clock_[client_id] = timestamp;
  // update slowest
  if (slowest_client_clock_ == -1 || timestamp < slowest_client_clock_) {
    slowest_client_clock_ = timestamp;
  }
  return 0;
}

int VectorClockST::Tick(int32_t client_id)
{
  if (IsSlowest(client_id)) {
    // is the slowest, update slowest clock.
    vec_clock_[client_id] += 1;
    return ++slowest_client_clock_;
  }
  // not the slowest. just tick.
  vec_clock_[client_id] += 1;
  return 0;
}

int32_t VectorClockST::get_client_clock(int32_t client_id) const
{
  return vec_clock_.at(client_id);
}

int32_t VectorClockST::get_slowest_clock() const
{
  return slowest_client_clock_;
}

// =========== Private Functions ============

bool VectorClockST::IsSlowest(int32_t client_id)
{
  if (vec_clock_[client_id] != slowest_client_clock_) {
    // definitely not the slowest.
    return false;
  }
  // check if it is also unique
  int num_slowest = 0;
  vec_clock_t::const_iterator it;
  for (it = vec_clock_.begin(); it != vec_clock_.end(); it++) {
    if (it->second == slowest_client_clock_) ++num_slowest;
    if (num_slowest > 1) return false;
  }
  return true;
}

}  // namespace petuum
