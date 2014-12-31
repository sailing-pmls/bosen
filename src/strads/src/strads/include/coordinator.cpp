

#include <string>
#include <iostream>
#include <deque>
#include <unordered_map>

#include <strads/include/common.hpp>
#include <strads/include/coordinator.hpp>
#include <glog/logging.h>

coordinator_machctx::coordinator_machctx(sharedctx *shctx):m_shctx(shctx){
  LOG(INFO) << "coordinator CONSTRUCTOR : " << this->m_shctx->rank;
}
