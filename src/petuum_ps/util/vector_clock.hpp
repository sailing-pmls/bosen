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
  // Return the minimum clock if client_id is the slowest thread; 0 if not;
  // negatives for error;
  virtual int32_t Tick(int32_t id);

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
