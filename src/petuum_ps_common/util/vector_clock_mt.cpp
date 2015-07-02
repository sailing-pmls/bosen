#include <petuum_ps_common/util/vector_clock_mt.hpp>
#include <vector>

namespace petuum {

VectorClockMT::VectorClockMT():
  VectorClock(){}

VectorClockMT::VectorClockMT(const std::vector<int32_t>& ids) :
  VectorClock(ids) { }

void VectorClockMT::AddClock(int32_t id, int32_t clock) {
  std::unique_lock<SharedMutex> write_lock(mutex_);
  VectorClock::AddClock(id, clock);
}

int32_t VectorClockMT::Tick(int32_t id) {
  std::unique_lock<SharedMutex> write_lock(mutex_);
  return VectorClock::Tick(id);
}

int32_t VectorClockMT::TickUntil(int32_t id, int32_t clock) {
  std::unique_lock<SharedMutex> write_lock(mutex_);
  return VectorClock::TickUntil(id, clock);
}

int32_t VectorClockMT::get_clock(int32_t id) const {
  std::unique_lock<SharedMutex> read_lock(mutex_);
  return VectorClock::get_clock(id);
}

int32_t VectorClockMT::get_min_clock() const {
  std::unique_lock<SharedMutex> read_lock(mutex_);
  return VectorClock::get_min_clock();
}

}  // namespace petuum
