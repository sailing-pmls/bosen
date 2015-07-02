// author: jinliang

#pragma once

#include <vector>
#include <map>
#include <glog/logging.h>
#include <boost/utility.hpp>

#include <petuum_ps_common/include/host_info.hpp>
#include <petuum_ps_common/include/abstract_row.hpp>
#include <petuum_ps_common/comm_bus/comm_bus.hpp>
#include <petuum_ps_common/include/configs.hpp>
#include <petuum_ps_common/util/vector_clock_mt.hpp>

namespace petuum {

// In petuum PS, thread is treated as first-class citizen. Some globaly
// shared thread information, such as ID, are stored in static variable to
// avoid having passing some variables every where.
class ThreadContext {
public:
  static void RegisterThread(int32_t thread_id) {
    thr_info_ = new Info(thread_id);
  }

  static int32_t get_id() {
    return thr_info_->entity_id_;
  }

  static int32_t get_clock() {
    return thr_info_->clock_;
  }

  static void Clock() {
    ++(thr_info_->clock_);
  }

  static int32_t GetCachedSystemClock() {
    return thr_info_->cached_system_clock_;
  }

  static void SetCachedSystemClock(int32_t system_clock) {
    thr_info_->cached_system_clock_ = system_clock;
  }

private:
  struct Info : boost::noncopyable {
    explicit Info(int32_t entity_id):
        entity_id_(entity_id),
        clock_(0),
        cached_system_clock_(0) { }

    ~Info(){ }

    const int32_t entity_id_;
    int32_t clock_;
    int32_t cached_system_clock_;
  };

  // We do not use thread_local here because there's a bug in
  // g++ 4.8.1 or lower: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=55800
  static __thread Info *thr_info_;
};


// Init must have "happens-before" relation with all other functions.
// After Init(), accesses to all other functions are concurrent.
class GlobalContext : boost::noncopyable {
public:
  // Functions that do not depend on Init()
  static int32_t get_thread_id_min(int32_t client_id) {
    return client_id*kMaxNumThreadsPerClient;
  }

  static int32_t get_thread_id_max(int32_t client_id) {
    return (client_id + 1)*kMaxNumThreadsPerClient - 1;
  }

  static int32_t get_name_node_id() {
    return 0;
  }

  static int32_t get_name_node_client_id() {
    return 0;
  }

  static bool am_i_name_node_client() {
    return (client_id_ == get_name_node_client_id());
  }

  static int32_t get_bg_thread_id(int32_t client_id, int32_t comm_channel_idx) {
    return get_thread_id_min(client_id) + kBgThreadIDStartOffset
        + comm_channel_idx;
  }

  static int32_t get_head_bg_id(int32_t client_id) {
    return get_bg_thread_id(client_id, 0);
  }

  static int32_t get_server_thread_id(int32_t client_id,
                                      int32_t comm_channel_idx) {
    return get_thread_id_min(client_id) + kServerThreadIDStartOffset
        + comm_channel_idx;
  }

  static size_t get_row_candidate_factor() {
    return row_candidate_factor_;
  }

  static void GetServerThreadIDs(int32_t comm_channel_idx,
                                 std::vector<int32_t> *server_thread_ids) {
    (*server_thread_ids).clear();
    for (int32_t i = 0; i < num_clients_; ++i) {
      (*server_thread_ids).push_back(get_server_thread_id(i, comm_channel_idx));
    }
  }

  static int32_t thread_id_to_client_id(int32_t thread_id) {
    return thread_id/kMaxNumThreadsPerClient;
  }

  static int32_t get_serialized_table_separator() {
    return -1;
  }

  static int32_t get_serialized_table_end() {
    return -2;
  }

  // "server" is different from name node.
  // Name node is not considered as server.
  static inline void Init(
      int32_t num_comm_channels_per_client,
      int32_t num_app_threads,
      int32_t num_table_threads,
      int32_t num_tables,
      int32_t num_clients,
      const std::map<int32_t, HostInfo> &host_map,
      int32_t client_id,
      int32_t server_ring_size,
      ConsistencyModel consistency_model,
      bool aggressive_cpu,
      int32_t snapshot_clock,
      const std::string &snapshot_dir,
      int32_t resume_clock,
      const std::string &resume_dir,
      UpdateSortPolicy update_sort_policy,
      long bg_idle_milli,
      double client_bandwidth_mbps,
      double server_bandwidth_mbps,
      size_t thread_oplog_batch_size,
      long server_idle_milli,
      int32_t row_candidate_factor,
      int32_t numa_index,
      NumaPolicy numa_policy,
      bool naive_table_oplog_meta,
      bool use_approx_sort,
      bool suppression_on) {

    num_comm_channels_per_client_
        = num_comm_channels_per_client;
    num_total_comm_channels_
        = num_comm_channels_per_client*num_clients;

    num_app_threads_ = num_app_threads;
    num_table_threads_ = num_table_threads;
    num_tables_ = num_tables;
    num_clients_ = num_clients;
    host_map_ = host_map;

    client_id_ = client_id;
    server_ring_size_ = server_ring_size;
    consistency_model_ = consistency_model;

    local_id_min_ = get_thread_id_min(client_id);
    aggressive_cpu_ = aggressive_cpu;

    snapshot_clock_ = snapshot_clock;
    snapshot_dir_ = snapshot_dir;
    resume_clock_ = resume_clock;
    resume_dir_ = resume_dir;
    update_sort_policy_ = update_sort_policy;
    bg_idle_milli_ = bg_idle_milli;

    client_bandwidth_mbps_ = client_bandwidth_mbps;
    server_bandwidth_mbps_ = server_bandwidth_mbps;
    thread_oplog_batch_size_ = thread_oplog_batch_size;

    server_idle_milli_ = server_idle_milli;

    row_candidate_factor_ = row_candidate_factor;

    numa_index_ = numa_index;

    numa_policy_ = numa_policy;

    naive_table_oplog_meta_ = naive_table_oplog_meta;

    use_approx_sort_ = use_approx_sort;

    suppression_on_ = suppression_on;

    for (auto host_iter = host_map.begin();
         host_iter != host_map.end(); ++host_iter) {
      HostInfo host_info = host_iter->second;
      int port_num = std::stoi(host_info.port, 0, 10);

      if (host_iter->first == get_name_node_id()) {
        name_node_host_info_ = host_info;

        ++port_num;
        std::stringstream ss;
        ss << port_num;
        host_info.port = ss.str();
      }

      for (int i = 0; i < num_comm_channels_per_client_; ++i) {
        int32_t server_id = get_server_thread_id(host_iter->first, i);
        server_map_.insert(std::make_pair(server_id, host_info));

        ++port_num;
        std::stringstream ss;
        ss << port_num;
        host_info.port = ss.str();

        server_ids_.push_back(server_id);
      }
    }
  }

  // Functions that depend on Init()
  static inline int32_t get_num_total_comm_channels() {
    return num_total_comm_channels_;
  }

  static int32_t get_num_comm_channels_per_client() {
    return num_comm_channels_per_client_;
  }

  // total number of application threads including init thread
  static inline int32_t get_num_app_threads() {
    return num_app_threads_;
  }

  // Total number of application threads that needs table access
  // num_app_threads = num_table_threads_ or num_app_threads_
  // = num_table_threads_ + 1
  static inline int32_t get_num_table_threads() {
    return num_table_threads_;
  }

  static inline int32_t get_head_table_thread_id() {
    int32_t init_thread_id = get_thread_id_min(client_id_) + kInitThreadIDOffset;
    return (num_table_threads_ == num_app_threads_) ?
        init_thread_id : init_thread_id + 1;
  }

  static inline int32_t get_num_tables() {
    return num_tables_;
  }

  static int32_t get_num_clients() {
    return num_clients_;
  }

  static int32_t get_num_total_servers() {
    return num_comm_channels_per_client_ * num_clients_;
  }

  static HostInfo get_server_info(int32_t server_id) {
    std::map<int32_t, HostInfo>::const_iterator iter
      = server_map_.find(server_id);
    CHECK(iter != server_map_.end()) << "id not found "
                                     << server_id;
    return iter->second;
  }

  static HostInfo get_name_node_info() {
    return name_node_host_info_;
  }

  static const std::vector<int32_t> &get_all_server_ids() {
    return server_ids_;
  }

  static int32_t get_client_id() {
    return client_id_;
  }

  static int32_t GetPartitionCommChannelIndex(int32_t row_id) {
    return row_id % num_comm_channels_per_client_;
  }

  // get the id of the server who is responsible for holding that row
  static int32_t GetPartitionClientID(int32_t row_id) {
    return (row_id / num_comm_channels_per_client_) % num_clients_;
  }

  static int32_t GetPartitionServerID(int32_t row_id,
                                      int32_t comm_channel_idx) {
    int32_t client_id = GetPartitionClientID(row_id);
    return get_server_thread_id(client_id, comm_channel_idx);
  }

  static int32_t GetCommChannelIndexServer(int32_t server_id) {
    int32_t index = server_id % kMaxNumThreadsPerClient
                    - kServerThreadIDStartOffset;
    return index;
  }

  static int32_t get_server_ring_size(){
    return server_ring_size_;
  }

  static ConsistencyModel get_consistency_model(){
    return consistency_model_;
  }

  static int32_t get_local_id_min(){
    return local_id_min_;
  }

  static bool get_aggressive_cpu() {
    return aggressive_cpu_;
  }

  // # locks in a StripedLock pool.
  static int32_t GetLockPoolSize() {
    static const int32_t kStripedLockExpansionFactor = 20;
    return (num_app_threads_ + num_comm_channels_per_client_)
        * kStripedLockExpansionFactor;
  }

  static int32_t GetLockPoolSize(size_t table_capacity) {
    static const int32_t kStripedLockReductionFactor = 1;
    return (table_capacity <= 2*kStripedLockReductionFactor)
        ? table_capacity
        : table_capacity / kStripedLockReductionFactor;
  }

  static int32_t get_snapshot_clock() {
    return snapshot_clock_;
  }

  static const std::string &get_snapshot_dir() {
    return snapshot_dir_;
  }

  static int32_t get_resume_clock() {
    return resume_clock_;
  }

  static const std::string &get_resume_dir() {
    return resume_dir_;
  }

  static UpdateSortPolicy get_update_sort_policy() {
    return update_sort_policy_;
  }

  static long get_bg_idle_milli() {
    return bg_idle_milli_;
  }

  static double get_client_bandwidth_mbps() {
    return client_bandwidth_mbps_;
  }

  static double get_server_bandwidth_mbps() {
    return server_bandwidth_mbps_;
  }

  static size_t get_thread_oplog_batch_size() {
    return thread_oplog_batch_size_;
  }

  static long get_server_idle_milli() {
    return server_idle_milli_;
  }

  static int32_t get_numa_index() {
    return numa_index_;
  }

  static NumaPolicy get_numa_policy() {
    return numa_policy_;
  }

  static bool get_naive_table_oplog_meta() {
    return naive_table_oplog_meta_;
  }

  static bool get_suppression_on() {
    return suppression_on_;
  }

  static bool get_use_aprox_sort() {
    return use_approx_sort_;
  }

  static CommBus* comm_bus;

  // name node thread id - 0
  // server thread ids - 1~99
  // bg thread ids - 100~199
  // init thread id - 200
  // app threads - 201~xxx

  static const int32_t kMaxNumThreadsPerClient = 1000;
  // num of server + name node thread per node <= 100
  static const int32_t kBgThreadIDStartOffset = 100;
  static const int32_t kInitThreadIDOffset = 200;
  static const int32_t kServerThreadIDStartOffset = 1;
private:
  static int32_t num_clients_;

  static int32_t num_comm_channels_per_client_;
  static int32_t num_total_comm_channels_;


  static int32_t num_app_threads_;
  static int32_t num_table_threads_;
  static int32_t num_tables_;

  static std::map<int32_t, HostInfo> host_map_;
  static std::map<int32_t, HostInfo> server_map_;
  static HostInfo name_node_host_info_;
  static std::vector<int32_t> server_ids_;

  static int32_t client_id_;
  static int32_t server_ring_size_;

  static ConsistencyModel consistency_model_;
  static int32_t local_id_min_;
  static bool aggressive_cpu_;

  static int32_t snapshot_clock_;
  static std::string snapshot_dir_;
  static int32_t resume_clock_;
  static std::string resume_dir_;
  static UpdateSortPolicy update_sort_policy_;
  static long bg_idle_milli_;

  static double client_bandwidth_mbps_;
  static double server_bandwidth_mbps_;

  static size_t thread_oplog_batch_size_;

  static long server_idle_milli_;

  static int32_t row_candidate_factor_;

  static int32_t numa_index_;

  static NumaPolicy numa_policy_;

  static bool naive_table_oplog_meta_;

  static bool suppression_on_;

  static bool use_approx_sort_;

  //static std::vector<CommBus*> comm_bus;
};

}   // namespace petuum
