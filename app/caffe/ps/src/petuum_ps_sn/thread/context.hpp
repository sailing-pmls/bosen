// author: jinliang

#pragma once

#include <vector>
#include <map>
#include <glog/logging.h>
#include <boost/thread/tss.hpp>
#include <boost/utility.hpp>
#include <limits.h>
#include <condition_variable>

#include <petuum_ps_common/include/abstract_row.hpp>
#include <petuum_ps_common/comm_bus/comm_bus.hpp>
#include <petuum_ps_common/include/configs.hpp>
#include <petuum_ps_common/util/vector_clock_mt.hpp>

namespace petuum {

// In petuum PS, thread is treated as first-class citizen. Some globaly
// shared thread information, such as ID, are stored in static variable to
// avoid having passing some variables every where.
class ThreadContextSN {
public:
  static void RegisterThread(int32_t thread_id) {
    thr_info_.reset(new Info(thread_id));
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

  static int32_t get_clock_ahead() {
    return thr_info_->clock_ahead_;
  }

  static void dec_clock_ahead() {
    --(thr_info_->clock_ahead_);
  }

  static void set_clock_ahead(int32_t clock_ahead) {
    thr_info_->clock_ahead_ = clock_ahead;
  }

private:
  struct Info {
    explicit Info(int32_t entity_id):
        entity_id_(entity_id),
        clock_(0),
	clock_ahead_(0) {}

    ~Info(){}

    const int32_t entity_id_;
    int32_t clock_;
    int32_t clock_ahead_;
  };

  static boost::thread_specific_ptr<Info> thr_info_;
};


// Init must have "happens-before" relation with all other functions.
// After Init(), accesses to all other functions are concurrent.
class GlobalContextSN : boost::noncopyable {
public:
  // Functions that do not depend on Init()
  static int32_t get_local_id_min() {
    return 0;
  }

  static int32_t get_local_id_max() {
    return INT_MAX;
  }

  // "server" is different from name node.
  // Name node is not considered as server.
  static inline void Init(int32_t num_app_threads,
      int32_t num_table_threads,
      int32_t num_tables,
      const std::string &ooc_path_prefix,
      ConsistencyModel consistency_model) {
    num_app_threads_ = num_app_threads;
    num_table_threads_ = num_table_threads;
    num_tables_ = num_tables;
    ooc_path_prefix_ = ooc_path_prefix;
    consistency_model_ = consistency_model;
  }

  // Functions that depend on Init()
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
  static inline int32_t get_num_tables() {
    return num_tables_;
  }

  // Cuckoo hash table will have minimal_capacity * cuckoo_expansion_factor_
  // since cuckoo hash performs poorly when nearly full.
  static inline float get_cuckoo_expansion_factor() {
    return kCuckooExpansionFactor;
  }

  static const std::string &get_ooc_path_prefix() {
    return ooc_path_prefix_;
  }

  static ConsistencyModel get_consistency_model() {
    return consistency_model_;
  }

  // # locks in a StripedLock pool.
  static int32_t GetLockPoolSize() {
    static const int32_t kStripedLockExpansionFactor = 20;
    return num_app_threads_ * kStripedLockExpansionFactor;
  }

  static int32_t GetLockPoolSize(size_t table_capacity) {
    static const int32_t kStripedLockReductionFactor = 20;
    return (table_capacity <= 2*kStripedLockReductionFactor)
        ? table_capacity
        : table_capacity / kStripedLockReductionFactor;
  }

  static const int32_t kInitThreadIDOffset = 100;
  static const int32_t kStripedLockExpansionFactor = 20;
  static float kCuckooExpansionFactor;
  static CommBus* comm_bus;

  static VectorClockMT vector_clock_;
  static std::mutex clock_mtx_;
  static std::condition_variable clock_cond_var_;

private:
  static int32_t num_app_threads_;
  static int32_t num_table_threads_;
  static int32_t num_tables_;
  static float cuckoo_expansion_factor_;
  static std::string ooc_path_prefix_;
  static ConsistencyModel consistency_model_;
};

}   // namespace petuum
