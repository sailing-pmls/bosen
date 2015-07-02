// author: jinliang

#pragma once
#include <petuum_ps/thread/bg_worker_group.hpp>

namespace petuum {
class SSPPushBgWorkerGroup : public BgWorkerGroup {
public:
  SSPPushBgWorkerGroup(std::map<int32_t, ClientTable* > *tables);
  ~SSPPushBgWorkerGroup() { }

  virtual int32_t GetSystemClock();
  virtual void WaitSystemClock(int32_t my_clock);

private:
  virtual void CreateBgWorkers();

  // Servers might not update all the rows in a client's process cache
  // (there could be a row that nobody writes to for many many clocks).
  // Therefore, we need a locally shared clock to denote the minimum clock that
  // the entire system has reached, which determines if a local thread may read
  // a local cached copy of the row.
  // Each bg worker maintains a vector clock for servers. The clock denotes the
  // clock that the server has reached, that is the minimum clock that the
  // server has seen from all clients.
  // This bg_server_clock_ contains one clock for each bg worker which is the
  // min server clock that the bg worker has seen.
  std::mutex system_clock_mtx_;
  std::condition_variable system_clock_cv_;

  std::atomic_int_fast32_t system_clock_;
  VectorClockMT bg_server_clock_;

};
}
