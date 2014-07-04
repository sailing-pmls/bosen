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
#include <petuum_ps/thread/row_request_oplog_mgr.hpp>
#include <utility>
#include <glog/logging.h>
#include <list>
#include <vector>

namespace petuum {

bool SSPRowRequestOpLogMgr::AddRowRequest(RowRequestInfo &request,
  int32_t table_id, int32_t row_id) {
  uint32_t version = request.version;
  request.sent = true;

  {
    std::pair<int32_t, int32_t> request_key(table_id, row_id);
    if (pending_row_requests_.count(request_key) == 0) {
      pending_row_requests_.insert(std::make_pair(request_key,
        std::list<RowRequestInfo>()));
      VLOG(0) << "pending row requests does not have this table_id = "
	      << table_id << " row_id = " << row_id;
    }
    std::list<RowRequestInfo> &request_list
      = pending_row_requests_[request_key];
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
  }

  {
    if (version_request_cnt_map_.count(version) == 0) {
      version_request_cnt_map_[version] = 0;
    }
    ++version_request_cnt_map_[version];
  }

  return request.sent;
}

int32_t SSPRowRequestOpLogMgr::InformReply(int32_t table_id, int32_t row_id,
  int32_t clock, uint32_t curr_version, std::vector<int32_t> *app_thread_ids) {
  (*app_thread_ids).clear();
  std::pair<int32_t, int32_t> request_key(table_id, row_id);
  std::list<RowRequestInfo> &request_list = pending_row_requests_[request_key];
  int32_t clock_to_request = -1;

  while (!request_list.empty()) {
    RowRequestInfo &request = request_list.front();
    if (request.clock <= clock) {
      // remove the request
      uint32_t req_version = request.version;
      app_thread_ids->push_back(request.app_thread_id);
      request_list.pop_front();

      // decrement the version count
      --version_request_cnt_map_[req_version];
      CHECK_GE(version_request_cnt_map_[req_version], 0);
      // if version count becomes 0, remove the count
      if (version_request_cnt_map_[req_version] == 0) {
        version_request_cnt_map_.erase(req_version);
	CleanVersionOpLogs(req_version, curr_version);
      }
    } else {
      if (!request.sent) {
	clock_to_request = request.clock;
	request.sent = true;
	uint32_t req_version = request.version;
	--version_request_cnt_map_[req_version];

	request.version = curr_version - 1;
	if (version_request_cnt_map_.count(request.version) == 0) {
	  version_request_cnt_map_[request.version] = 0;
	}
	++version_request_cnt_map_[request.version];

	if (version_request_cnt_map_[req_version] == 0) {
	  version_request_cnt_map_.erase(req_version);
	  CleanVersionOpLogs(req_version, curr_version);
	}
      }
      break;
    }
  }
  // if there's no request in that list, I can remove the empty list
  if (request_list.empty())
    pending_row_requests_.erase(request_key);
  return clock_to_request;
}

bool SSPRowRequestOpLogMgr::AddOpLog(uint32_t version, BgOpLog *oplog) {
  CHECK_EQ(version_oplog_map_.count(version), (size_t) 0)
     << "version number has wrapped"
     << " around, the system does not how to deal with it. "
     << "Maybe use a larger version number?";
  // There are pending requests, they are from some older version or the current
  // version, so I need to save the oplog for them.
  if (version_request_cnt_map_.size() > 0) {
    version_oplog_map_[version] = oplog;
    return true;
  }
  return false;
}

BgOpLog *SSPRowRequestOpLogMgr::GetOpLog(uint32_t version) {
  auto iter = version_oplog_map_.find(version);
  CHECK(iter != version_oplog_map_.end());
  return iter->second;
}

void SSPRowRequestOpLogMgr::CleanVersionOpLogs(uint32_t req_version,
  uint32_t curr_version) {

  // All oplogs that are saved must be of an earlier version than current
  // version, while a request could be from the current version.

  // The first version to be removed is current version, which is not yet stored
  // So nothing to remove.
  if (req_version + 1 == curr_version)
    return;

  // First, make sure there's no request from a previous version.
  // We do that by checking if there's an OpLog of this version,
  // if there is one, it must be save for some older requests.
  if (version_oplog_map_.count(req_version) > 0)
    return;

  uint32_t version_to_remove = req_version;
  do {
    // No previous OpLog, can remove a later version of oplog.
    delete version_oplog_map_[version_to_remove + 1];
    version_oplog_map_.erase(version_to_remove + 1);
    ++version_to_remove;
    // Figure out how many later versions of oplogs can be removed.
  } while((version_request_cnt_map_.count(version_to_remove) == 0)
    && (version_to_remove != curr_version));
}

BgOpLog *SSPRowRequestOpLogMgr::OpLogIterInit(uint32_t start_version,
  uint32_t end_version) {
  oplog_iter_version_st_ = start_version;
  oplog_iter_version_end_ = end_version;
  oplog_iter_version_next_ = oplog_iter_version_st_ + 1;
  return GetOpLog(start_version);
}


BgOpLog *SSPRowRequestOpLogMgr::OpLogIterNext(uint32_t *version) {
  if (oplog_iter_version_next_ > oplog_iter_version_end_)
    return NULL;
  *version = oplog_iter_version_next_;
  ++oplog_iter_version_next_;
  return GetOpLog(*version);
}


}  // namespace petuum
