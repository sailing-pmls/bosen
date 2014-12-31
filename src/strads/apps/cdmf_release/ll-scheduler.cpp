#include <stdio.h>
#include <strads/include/common.hpp>
#include <strads/util/utility.hpp>

#include <iostream>     // std::cout
#include <algorithm>    // std::for_each
#include <vector>       // std::vector

#include <glog/logging.h>
#include <mpi.h>

using namespace std;

void *scheduler_mach(void *arg){
  sharedctx *ctx = (sharedctx *)arg;
  strads_msg(INF, "STRADS SCHEDULER rank %d \n", ctx->rank);
  return NULL;
}
