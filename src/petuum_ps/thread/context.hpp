// author: jinliang

#pragma once

#include <vector>
#include <map>
#include <glog/logging.h>
#include <boost/thread/tss.hpp>
#include <boost/utility.hpp>

#include "petuum_ps/include/host_info.hpp"
#include "petuum_ps/include/abstract_row.hpp"
#include "petuum_ps/comm_bus/comm_bus.hpp"
#include "petuum_ps/include/configs.hpp"
#include "petuum_ps/util/vector_clock_mt.hpp"

namespace petuum {

// In petuum PS, thread is treated as first-class citizen. Some globaly
// shared thread information, such as ID, are stored in static variable to
// avoid having passing some variables every where.
class ThreadContext {
public:
  static void RegisterThread(int32_t thread_id) {
    thr_info_.reset(new Info(thread_id));
  }

  static int32_t get_id() {
    return thr_info_->entity_id_;
  }

  static bool get_read_process_cache() {
    return thr_info_->read_process_cache_;
  }

  static void set_read_process_cache(bool read_process_cache) {
    thr_info_->read_process_cache_ = read_process_cache;
  }

  static int32_t get_clock() {
    return thr_info_->clock_;
  }

  static void Clock() {
    ++(thr_info_->clock_);
  }

private:
  struct Info {
    explicit Info(int32_t entity_id):
        entity_id_(entity_id),
        clock_(0),
        read_process_cache_(true) {}

    ~Info(){}

    const int32_t entity_id_;
    int32_t clock_;

    /* Data members needed for server push */
    // If the process cache is fresh enough for the application thread
    // to read. It is updated every time this thread calls Clock().
    bool read_process_cache_;
  };

  static boost::thread_specific_ptr<Info> thr_info_;
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

  static int32_t get_head_bg_id(int32_t client_id) {
    return get_thread_id_min(client_id) + kBgThreadIDStartOffset;
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
  static inline void Init(int32_t num_servers,
      int32_t num_local_server_threads,
      int32_t num_app_threads,
      int32_t num_table_threads,
      int32_t num_bg_threads,
      int32_t num_total_bg_threads,
      int32_t num_tables,
      int32_t num_clients,
      const std::vector<int32_t> &server_ids,
      const std::map<int32_t, HostInfo> &host_map,
      int32_t client_id,
      int32_t server_ring_size,
      ConsistencyModel consistency_model,
      bool aggressive_cpu) {
    num_servers_ = num_servers;
    num_local_server_threads_ = num_local_server_threads,
    num_app_threads_ = num_app_threads;
    num_table_threads_ = num_table_threads;
    num_bg_threads_ = num_bg_threads;
    num_total_bg_threads_ = num_total_bg_threads;
    num_tables_ = num_tables;
    num_clients_ = num_clients;
    server_ids_ = server_ids;
    host_map_ = host_map;
    client_id_ = client_id;
    server_ring_size_ = server_ring_size;
    consistency_model_ = consistency_model;
    local_id_min_ = get_thread_id_min(client_id);
    aggressive_cpu_ = aggressive_cpu;
  }

  // Functions that depend on Init()
  static inline int32_t get_num_servers() {
    return num_servers_;
  }

  static int32_t get_num_local_server_threads() {
    return num_local_server_threads_;
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

  static inline int32_t get_num_bg_threads() {
    return num_bg_threads_;
  }

  static int32_t get_num_total_bg_threads() {
    return num_total_bg_threads_;
  }

  static inline int32_t get_num_tables() {
    return num_tables_;
  }

  static int32_t get_num_clients() {
    return num_clients_;
  }

  // # locks in a StripedLock pool.
  static int32_t get_lock_pool_size() {
    return (num_bg_threads_ + num_app_threads_)*kStripedLockExpansionFactor;
  }

  // Cuckoo hash table will have minimal_capacity * cuckoo_expansion_factor_
  // since cuckoo hash performs poorly when nearly full.
  static inline float get_cuckoo_expansion_factor() {
    return cuckoo_expansion_factor_;
  }

  static HostInfo get_host_info(int32_t entity_id) {
    std::map<int32_t, HostInfo>::const_iterator iter
      = host_map_.find(entity_id);
    CHECK(iter != host_map_.end());

    return iter->second;
  }

  static int32_t get_client_id(){
    return client_id_;
  }

  static std::vector<int32_t> &get_server_ids(){
    return server_ids_;
  }

  static int32_t GetBgPartitionNum(int32_t row_id) {
    return row_id % num_bg_threads_;
  }

  // get the id of the server who is responsible for holding that row
  static int32_t GetRowPartitionServerID(int32_t table_id, int32_t row_id){
    int32_t server_id_idx = row_id % num_servers_;
    //VLOG(0) << "server_idx_ = " << server_id_idx;
    return server_ids_[server_id_idx];
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

  static CommBus* comm_bus;

  static const int32_t kMaxNumThreadsPerClient = 1000;
  // num of server + name node thread per node <= 100
  static const int32_t kBgThreadIDStartOffset = 100;
  static const int32_t kInitThreadIDOffset = 200;
  static const int32_t kStripedLockExpansionFactor = 20;
private:
  static int32_t num_servers_;
  static int32_t num_local_server_threads_;
  static int32_t num_app_threads_;
  static int32_t num_table_threads_;
  static int32_t num_bg_threads_;
  static int32_t num_total_bg_threads_;
  static int32_t num_tables_;
  static int32_t num_clients_;
  static int32_t lock_pool_size_;
  static float cuckoo_expansion_factor_;
  static std::map<int32_t, HostInfo> host_map_;
  static int32_t client_id_;
  static std::vector<int32_t> server_ids_;
  static int32_t server_ring_size_;

  static ConsistencyModel consistency_model_;
  static int32_t local_id_min_;
  static bool aggressive_cpu_;
};

}   // namespace petuum
