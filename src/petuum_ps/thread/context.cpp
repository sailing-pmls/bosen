#include "petuum_ps/thread/context.hpp"

namespace petuum {

boost::thread_specific_ptr<ThreadContext::Info> ThreadContext::thr_info_;

CommBus* GlobalContext::comm_bus;

int32_t GlobalContext::num_clients_ = 0;

int32_t GlobalContext::num_servers_ = 1;

int32_t GlobalContext::num_local_server_threads_ = 1;

int32_t GlobalContext::num_app_threads_ = 1;

int32_t GlobalContext::num_table_threads_ = 1;

int32_t GlobalContext::num_bg_threads_ = 1;

int32_t GlobalContext::num_total_bg_threads_ = 1;

int32_t GlobalContext::num_tables_ = 1;

int32_t GlobalContext::lock_pool_size_ = (GlobalContext::num_bg_threads_ +
    GlobalContext::num_app_threads_) * 20;

// Cuckoo hash performs better when it is less than 70% full (1/0.7=1.428).
float GlobalContext::cuckoo_expansion_factor_ = 1.428;

std::map<int32_t, HostInfo> GlobalContext::host_map_;

int32_t GlobalContext::client_id_ = 0;

std::vector<int32_t> GlobalContext::server_ids_;

int32_t GlobalContext::server_ring_size_;

ConsistencyModel GlobalContext::consistency_model_;

int32_t GlobalContext::local_id_min_;

bool GlobalContext::aggressive_cpu_;
}   // namespace petuum
