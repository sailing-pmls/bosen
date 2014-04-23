#include "petuum_ps/util/vector_clock.hpp"

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
