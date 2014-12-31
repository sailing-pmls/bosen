

#include <string>
#include <iostream>
#include <deque>
#include <unordered_map>

#include <strads/include/common.hpp>
#include <strads/include/worker.hpp>
#include <glog/logging.h>

worker_machctx::worker_machctx(sharedctx *shctx):m_shctx(shctx){
  LOG(INFO) << "Worker CONSTRUCTOR : " << this->m_shctx->rank;
}
