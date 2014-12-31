#include <string>
#include <iostream>
#include <deque>
#include <unordered_map>

#include <strads/include/common.hpp>
#include <strads/include/scheduler.hpp>
#include <glog/logging.h>

scheduler_machctx::scheduler_machctx(sharedctx *shctx):m_shctx(shctx){
  LOG(INFO) << "Scheduler CONSTRUCTOR : " << this->m_shctx->rank;
}
