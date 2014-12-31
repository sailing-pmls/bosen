#pragma once

#ifdef PETUUM_NUMA

#include <petuum_ps/thread/context.hpp>
#include <stdint.h>

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <sys/syscall.h>
#include <numa.h>
#include <numaif.h>

#include <glog/logging.h>
#include <sched.h>

namespace petuum {

class NumaMgr {
public:
  static void Init(bool set_affinity) {
    set_affinity_ = set_affinity;

    if (!set_affinity_)
      return;

    num_cpus_ = numa_num_configured_cpus();
    num_mem_nodes_ = numa_num_configured_nodes();
    LOG(INFO) << "num_cpus = " << num_cpus_
              << " num_mem_nodes = " << num_mem_nodes_;
  }

  static void ConfigureBgWorker() {
    if (!set_affinity_)
      return;

    int32_t client_id = GlobalContext::get_client_id();

    int32_t idx = ThreadContext::get_id() - GlobalContext::get_head_bg_id(
        client_id);

    int32_t node_id = idx % num_mem_nodes_;
    CHECK_EQ(numa_run_on_node(node_id), 0);

    struct bitmask *mask = numa_allocate_nodemask();
    mask = numa_bitmask_setbit(mask, node_id);

    // set NUMA zone binding to be prefer
    numa_set_bind_policy(0);
    numa_set_membind(mask);
    numa_free_nodemask(mask);
  }

  static void ConfigureServerThread() {
    if (!set_affinity_)
      return;

    int32_t client_id = GlobalContext::get_client_id();

    int32_t idx = ThreadContext::get_id() - GlobalContext::get_server_thread_id(
        client_id, 0);

    int32_t node_id = idx % num_mem_nodes_;
    CHECK_EQ(numa_run_on_node(node_id), 0);

    struct bitmask *mask = numa_allocate_nodemask();
    mask = numa_bitmask_setbit(mask, node_id);

    // set NUMA zone binding to be prefer
    numa_set_bind_policy(0);
    numa_set_membind(mask);
    numa_free_nodemask(mask);
  }

  static void ConfigureTableThread() {
    if (!set_affinity_)
      return;

    int32_t idx = ThreadContext::get_id() - GlobalContext::get_head_table_thread_id();
    int32_t node_id = idx % num_mem_nodes_;
    CHECK_EQ(numa_run_on_node(node_id), 0);

    struct bitmask *mask = numa_allocate_nodemask();
    mask = numa_bitmask_setbit(mask, node_id);

    // set NUMA zone binding to be prefer
    numa_set_bind_policy(0);
    numa_set_membind(mask);
    numa_free_nodemask(mask);
  }

private:
  static bool set_affinity_;
  static size_t num_cpus_;
  static size_t num_mem_nodes_;
};

}

#else

#include <stdint.h>

namespace petuum {

class NumaMgr {
public:
  static void Init(bool set_affinity) { }

  static void ConfigureBgWorker() { }

  static void ConfigureServerThread() { }

  static void ConfigureTableThread() { }

private:
};

}

#endif
