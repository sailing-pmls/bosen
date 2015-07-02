#pragma once
#include <stdint.h>
#include <vector>
#include <boost/noncopyable.hpp>
#include <boost/unordered_map.hpp>

namespace petuum {
// Assume that (#versions per iteration) * staleness < 2^32 -1.
class ServerVersionMgr : boost::noncopyable {
public:
  ServerVersionMgr(const std::vector<int32_t> &server_ids);
  ~ServerVersionMgr() { }

  // Server versions have an upper bound which is the version number
  // of the bg worker. Increment that whenever bg worker's version number
  // is incremented.
  void IncVersionUpperBound();

  // Set the version number for server - server_id.
  // Return the minimum version number if it has changed.
  // Server version monotonically increases.
  bool SetServerVersion(int32_t server_id, uint32_t version);
  uint32_t GetMinVersion();
  uint32_t GetVersionUpperBound();
private:
  bool IsUniqueMin(int32_t server_id);
  boost::unordered_map<int32_t, uint32_t> version_map_;
  uint32_t version_upper_bound_;
  uint32_t min_version_;
};
}
