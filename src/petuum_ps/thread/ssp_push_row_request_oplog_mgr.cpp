// Copyright (c) 2014, Sailing Lab
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the <ORGANIZATION> nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
// author: jinliang
#include <petuum_ps/thread/ssp_push_row_request_oplog_mgr.hpp>
#include <petuum_ps/thread/context.hpp>

#include <sstream>

namespace petuum {

bool SSPPushRowRequestOpLogMgr::AddRowRequest(RowRequestInfo &request,
  int32_t table_id, int32_t row_id) {
  request.sent = true;

  std::pair<int32_t, int32_t> request_key(table_id, row_id);
  if (pending_row_requests_.count(request_key) == 0) {
    pending_row_requests_.insert(std::make_pair(request_key,
      std::list<RowRequestInfo>()));
  }
  std::list<RowRequestInfo> &request_list = pending_row_requests_[request_key];
  bool request_added = false;
  // Requests are sorted in increasing order of clock number.
  // When a request is to be inserted, start from the end as the requst's
  // clock is more likely to be larger.
  for (auto iter = request_list.end(); iter != request_list.begin(); iter--) {
    auto iter_prev = std::prev(iter);
    int32_t clock = request.clock;
    if (clock >= iter_prev->clock) {
      VLOG(0) << "I'm requesting clock is " << clock
              << " There's a previous request requesting clock "
              << iter_prev->clock;
      // insert before iter
      request.sent = false;
      request_list.insert(iter, request);
      request_added = true;
      break;
    }
  }
  if (!request_added)
    request_list.push_front(request);

  return request.sent;
}

int32_t SSPPushRowRequestOpLogMgr::InformReply(int32_t table_id, int32_t row_id,
  int32_t clock, uint32_t curr_version, std::vector<int32_t> *app_thread_ids) {
  (*app_thread_ids).clear();
  std::pair<int32_t, int32_t> request_key(table_id, row_id);
  std::list<RowRequestInfo> &request_list = pending_row_requests_[request_key];
  int32_t clock_to_request = -1;

  while (!request_list.empty()) {
    RowRequestInfo &request = request_list.front();
    if (request.clock <= clock) {
      // remove the request
      app_thread_ids->push_back(request.app_thread_id);
      request_list.pop_front();
    } else {
      if (!request.sent) {
	clock_to_request = request.clock;
	request.sent = true;
      }
      break;
    }
  }
  // if there's no request in that list, I can remove the empty list
  if (request_list.empty())
    pending_row_requests_.erase(request_key);
  return clock_to_request;
}

// version number monotonically increases
bool SSPPushRowRequestOpLogMgr::AddOpLog(uint32_t version, BgOpLog *oplog) {
  version_oplog_list_.push_back(std::pair<uint32_t, BgOpLog*>(version, oplog));
  return true;
}

BgOpLog *SSPPushRowRequestOpLogMgr::GetOpLog(uint32_t version) {
  for (auto iter = version_oplog_list_.cbegin();
       iter != version_oplog_list_.cend(); iter++) {
    if(iter->first == version)
      return iter->second;
  }
  LOG(FATAL) << "OpLog not found! version = " << version;
  return NULL;
}

void SSPPushRowRequestOpLogMgr::InformVersionInc() {
  server_version_mgr_.IncVersionUpperBound();
}

// Remove oplogs that are before this version.
void SSPPushRowRequestOpLogMgr::ServerAcknowledgeVersion(int32_t server_id,
                                                         uint32_t version) {

  if (!server_version_mgr_.SetServerVersion(server_id, version)) {
    //VLOG(0) << "Set server " << server_id << " version to "<< version
    //	    << " MinVersion does not change";
    return;
  }
  uint32_t version_to_remove = server_version_mgr_.GetMinVersion();

  VLOG(0) << "server id  = " << server_id << " version to remove = " << version_to_remove;
  if (version_oplog_list_.empty())
    return;

  uint32_t version_upper_bound = server_version_mgr_.GetVersionUpperBound();

  do {
    std::pair<uint32_t, BgOpLog*> &version_oplog = version_oplog_list_.front();
    if (version_to_remove > version_upper_bound) {
      if (version_oplog.first > version_upper_bound
	  && version_oplog.first <= version_to_remove) {
	delete version_oplog.second;
	version_oplog_list_.pop_front();
      } else {
	break;
      }
    } else {
      if (version_oplog.first > version_upper_bound) {
	delete version_oplog.second;
	version_oplog_list_.pop_front();
      } else if (version_oplog.first <= version_to_remove) {
	delete version_oplog.second;
	version_oplog_list_.pop_front();
      } else {
	break;
      }
    }
    if (version_oplog_list_.empty())
      break;

    version_oplog = version_oplog_list_.front();
  } while (1);

}

BgOpLog *SSPPushRowRequestOpLogMgr::OpLogIterInit(uint32_t start_version,
                                                  uint32_t end_version) {
  oplog_iter_version_st_ = start_version;
  oplog_iter_version_end_ = end_version;
  oplog_iter_version_next_ = oplog_iter_version_st_ + 1;

  //std::stringstream ss;

  for (oplog_iter_ = version_oplog_list_.cbegin();
       oplog_iter_ != version_oplog_list_.cend(); oplog_iter_++) {
    if(oplog_iter_->first == oplog_iter_version_st_) {
      return oplog_iter_->second;
    }

    //ss << "oplog_iter_v=" << oplog_iter_->first << ";";
  }
  LOG(FATAL) << "OpLog not found! version = " << start_version
	     << " end version = " << end_version
	     << " version_oplog_list_.size() = " << version_oplog_list_.size()
    //<< " " << ss.str()
	     << " client id = " << GlobalContext::get_client_id();
  return NULL;
}
BgOpLog *SSPPushRowRequestOpLogMgr::OpLogIterNext(uint32_t *version) {
  if (oplog_iter_version_next_ > oplog_iter_version_end_)
    return NULL;
  oplog_iter_++;
  CHECK(oplog_iter_ != version_oplog_list_.cend());
  CHECK(oplog_iter_->first == oplog_iter_version_next_);
  *version = oplog_iter_->first;
  ++oplog_iter_version_next_;
  return oplog_iter_->second;
}

}  // namespace petuum
