#ifdef PETUUM_NUMA

#include <petuum_ps/thread/numa_mgr.hpp>

namespace petuum {

bool NumaMgr::set_affinity_ = false;
std::unique_ptr<AbstractNumaMgrObj> NumaMgr::numa_obj_;

}

#endif
