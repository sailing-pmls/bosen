#include <petuum_ps_sn/thread/context.hpp>

namespace petuum {

boost::thread_specific_ptr<ThreadContextSN::Info> ThreadContextSN::thr_info_;

const int32_t GlobalContextSN::kInitThreadIDOffset;

CommBus* GlobalContextSN::comm_bus;

int32_t GlobalContextSN::num_app_threads_ = 1;

int32_t GlobalContextSN::num_table_threads_ = 1;

int32_t GlobalContextSN::num_tables_ = 1;

VectorClockMT GlobalContextSN::vector_clock_;

std::mutex GlobalContextSN::clock_mtx_;

std::condition_variable GlobalContextSN::clock_cond_var_;

std::string GlobalContextSN::ooc_path_prefix_;

ConsistencyModel GlobalContextSN::consistency_model_;

}   // namespace petuum
