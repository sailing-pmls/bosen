#include <petuum_ps/thread/context.hpp>

namespace petuum {

__thread ThreadContext::Info *ThreadContext::thr_info_;

CommBus* GlobalContext::comm_bus;

int32_t GlobalContext::num_clients_ = 0;

int32_t GlobalContext::num_comm_channels_per_client_;

int32_t GlobalContext::num_total_comm_channels_;

int32_t GlobalContext::num_app_threads_ = 1;

int32_t GlobalContext::num_table_threads_ = 1;

int32_t GlobalContext::num_tables_ = 1;

std::map<int32_t, HostInfo> GlobalContext::host_map_;

std::map<int32_t, HostInfo> GlobalContext::server_map_;

std::vector<int32_t> GlobalContext::server_ids_;

HostInfo GlobalContext::name_node_host_info_;

int32_t GlobalContext::client_id_ = 0;

int32_t GlobalContext::server_ring_size_;

ConsistencyModel GlobalContext::consistency_model_;

int32_t GlobalContext::local_id_min_;

bool GlobalContext::aggressive_cpu_;

int32_t GlobalContext::snapshot_clock_;

std::string GlobalContext::snapshot_dir_;

int32_t GlobalContext::resume_clock_;

std::string GlobalContext::resume_dir_;

UpdateSortPolicy GlobalContext::update_sort_policy_;

long GlobalContext::bg_idle_milli_;

double GlobalContext::bandwidth_mbps_;

size_t GlobalContext::oplog_push_upper_bound_kb_;

int32_t GlobalContext::oplog_push_staleness_tolerance_;

size_t GlobalContext::thread_oplog_batch_size_;

size_t GlobalContext::server_push_row_threshold_;

long GlobalContext::server_idle_milli_;

int32_t GlobalContext::server_row_candidate_factor_;

}   // namespace petuum
