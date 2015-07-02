#pragma once

#include <glog/logging.h>
#include <boost/unordered_map.hpp>
#include <vector>

namespace petuum {

// VectorClock manages a vector of clocks and maintains the minimum of these
// clocks. This class is single thread (ST) only.
class MaxVectorClock {
public:
  MaxVectorClock();

  // Add a clock in vector clock with initial timestampe. id must be unique.
  // Return 0 on success, negatives on error (e.g., duplicated id).
  virtual void AddClock(int32_t id, int32_t clock = 0);

  // return true if the clock has changed
  virtual bool TickUntil(int32_t id, int32_t clock);

  // Getters
  virtual int32_t get_clock(int32_t id) const;
  virtual int32_t get_min_clock() const;

  // If the tick of this client will change the slowest_client_clock_, then
  // it returns true.
  bool IsUniqueMax(int32_t clock);

private:
  boost::unordered_map<int32_t, int32_t> vec_clock_;
};

}  // namespace petuum
