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

  if (!IsUniqueMin(server_id)) {
    version_map_[server_id] = version;
    return false;
  }

  // VLOG(0) << "IsUniqueMin!! server id = " << server_id
  //	  << " version = " << version;
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
