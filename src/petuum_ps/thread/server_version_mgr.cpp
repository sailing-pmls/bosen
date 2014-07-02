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
#include <petuum_ps/thread/server_version_mgr.hpp>
#include <glog/logging.h>

namespace petuum {

ServerVersionMgr::ServerVersionMgr(const std::vector<int32_t> &server_ids) :
    version_upper_bound_(-1),
    min_version_(-1) {
  for (auto iter = server_ids.cbegin(); iter != server_ids.cend(); iter++) {
    version_map_[*iter] = -1;
  }
}

void ServerVersionMgr::IncVersionUpperBound() {
  ++version_upper_bound_;
}

bool ServerVersionMgr::SetServerVersion(int32_t server_id,
                                        uint32_t version) {
  if (version_map_[server_id] == version)
    return false;

  VLOG(0) << "Server id = " << server_id
	  << " original server version = "
	  << version_map_[server_id]
	  << "Set server version = " << version;

  if (!IsUniqueMin(server_id)) {
    version_map_[server_id] = version;
    return false;
  }

  VLOG(0) << "IsUniqueMin!! server id = " << server_id
	  << " version = " << version;
  version_map_[server_id] = version;
  // figure out what min_version_ should be
  if (min_version_ < version_upper_bound_) {
    CHECK_LE(version, version_upper_bound_);
    uint32_t min_version = version;
    for (auto iter = version_map_.cbegin(); iter != version_map_.cend();
         iter++) {
      if (iter->second < min_version) {
        min_version = iter->second;
	//min_version = version;
      }
    }
    min_version_ = min_version;
  } else {
    uint32_t min_version = version;
    for (auto iter = version_map_.cbegin(); iter != version_map_.cend();
         iter++) {

      if ((min_version > version_upper_bound_
           && (iter->second > version_upper_bound_ && iter->second < min_version))
           || (min_version <= version_upper_bound_
               && (iter->second > version_upper_bound_
                   || (iter->second <= version_upper_bound_
                       && iter->second < min_version)))) {
        min_version = iter->second;
      }
    }
    min_version_ = min_version;
  }
  VLOG(0) << "New min_version_ = " << min_version_;
  return true;
}

uint32_t ServerVersionMgr::GetMinVersion() {
  return min_version_;
}

uint32_t ServerVersionMgr::GetVersionUpperBound() {
  return version_upper_bound_;
}

bool ServerVersionMgr::IsUniqueMin(int32_t server_id) {
  if (version_map_[server_id] != min_version_) {
    // definitely not the slowest.
    return false;
  }
  // check if it is also unique
  int num_min = 0;
  for (auto iter = version_map_.cbegin(); iter != version_map_.cend(); iter++) {
    if (iter->second == min_version_) ++num_min;
    if (num_min > 1) return false;
  }
  return true;
}

}
