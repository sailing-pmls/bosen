#include <petuum_ps_common/util/max_vector_clock.hpp>

namespace petuum {

MaxVectorClock::MaxVectorClock() { }

void MaxVectorClock::AddClock(int32_t id, int32_t clock) {
  vec_clock_[id] = clock;
}

bool MaxVectorClock::TickUntil(int32_t id, int32_t clock) {
  auto iter = vec_clock_.find(id);
  CHECK(iter != vec_clock_.end()) << "id = " << id;

  if (iter->second == clock)
    return false;

  iter->second = clock;
  return true;
}

int32_t MaxVectorClock::get_clock(int32_t id) const
{
  auto iter = vec_clock_.find(id);
  CHECK(iter != vec_clock_.end()) << "id = " << id;
  return iter->second;
}

int32_t MaxVectorClock::get_min_clock() const {
  int32_t min_clock = INT_MAX;

  for (const auto &clock_pair : vec_clock_) {
    if (min_clock <= clock_pair.second)
      continue;
    min_clock = clock_pair.second;
  }

  return min_clock;
}

bool MaxVectorClock::IsUniqueMax(int32_t clock) {
  for (const auto &pair : vec_clock_) {
    if (clock <= pair.second)
      return false;
  }
  return true;
}

}  // namespace petuum
