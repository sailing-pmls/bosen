#ifdef PETUUM_NUMA

#include <petuum_ps/thread/numa_mgr.hpp>

namespace petuum {

bool NumaMgr::set_affinity_ = false;
size_t NumaMgr::num_cpus_ = 1;
size_t NumaMgr::num_mem_nodes_ = 1;

}

#endif
