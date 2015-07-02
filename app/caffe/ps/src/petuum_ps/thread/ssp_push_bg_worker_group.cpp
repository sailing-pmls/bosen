#include <petuum_ps/thread/ssp_push_bg_worker_group.hpp>
#include <petuum_ps/thread/ssp_push_bg_worker.hpp>
#include <petuum_ps/thread/ssp_aggr_bg_worker.hpp>
#include <petuum_ps/thread/context.hpp>

namespace petuum {

SSPPushBgWorkerGroup::SSPPushBgWorkerGroup(
    std::map<int32_t, ClientTable* > *tables):
    BgWorkerGroup(tables) {
  for (int32_t i = 0;
       i < GlobalContext::get_num_comm_channels_per_client(); ++i) {
    bg_server_clock_.AddClock(bg_worker_id_st_ + i, 0);
  }
}

void SSPPushBgWorkerGroup::CreateBgWorkers() {
  switch (GlobalContext::get_consistency_model()) {
    case SSPPush:
      {
        int32_t idx = 0;
        for (auto &worker : bg_worker_vec_) {
          worker = new SSPPushBgWorker(bg_worker_id_st_ + idx, idx, tables_,
                                       &init_barrier_, &create_table_barrier_,
                                       &system_clock_, &system_clock_mtx_,
                                       &system_clock_cv_,
                                       &bg_server_clock_);
          ++idx;
        }
      }
      break;
    case SSPAggr:
      {
        int32_t idx = 0;
        for (auto &worker : bg_worker_vec_) {
          worker = new SSPAggrBgWorker(bg_worker_id_st_ + idx, idx, tables_,
                                       &init_barrier_, &create_table_barrier_,
                                       &system_clock_, &system_clock_mtx_,
                                       &system_clock_cv_,
                                       &bg_server_clock_);
          ++idx;
        }
      }
      break;
    default:
      LOG(FATAL) << "Unsupported consistency model "
                 << GlobalContext::get_consistency_model();
  }
}

int32_t SSPPushBgWorkerGroup::GetSystemClock() {
  return static_cast<int32_t>(system_clock_.load());
}

void SSPPushBgWorkerGroup::WaitSystemClock(int32_t my_clock) {
  std::unique_lock<std::mutex> lock(system_clock_mtx_);
  // The bg threads might have advanced the clock after my last check.
  while (static_cast<int32_t>(system_clock_.load()) < my_clock) {
    system_clock_cv_.wait(lock);
  }
}

}
