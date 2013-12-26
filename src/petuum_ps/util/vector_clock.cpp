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

#include "petuum_ps/util/vector_clock.hpp"

namespace petuum {

VectorClock::VectorClock() : VectorClockST() { }

VectorClock::VectorClock(const VectorClock& other) : VectorClockST(other) { }

VectorClock& VectorClock::operator=(const VectorClock& rhs) {
  VectorClockST::operator=(rhs);
  return *this;
}

VectorClock::VectorClock(const std::vector<int32_t>& client_ids) :
  VectorClockST(client_ids) { }

int VectorClock::AddClock(int32_t client_id, int32_t timestamp)
{
  boost::unique_lock<boost::shared_mutex> write_lock(mutex_);
  return VectorClockST::AddClock(client_id, timestamp);
}

int VectorClock::Tick(int32_t client_id)
{
  boost::unique_lock<boost::shared_mutex> write_lock(mutex_);
  return VectorClockST::Tick(client_id);
}

int32_t VectorClock::get_client_clock(int32_t client_id) const
{
  boost::shared_lock<boost::shared_mutex> read_lock(mutex_);
  return VectorClockST::get_client_clock(client_id);
}

int32_t VectorClock::get_slowest_clock() const
{
  boost::shared_lock<boost::shared_mutex> read_lock(mutex_);
  return VectorClockST::get_slowest_clock();
}

}  // namespace petuum
