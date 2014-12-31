#include "petuum_ps/thread/row_request_mgr.hpp"
#include <utility>
#include <glog/logging.h>
#include <list>
#include <vector>

namespace petuum {

bool RowRequestMgr::AddRowRequest(RowRequestInfo &request,
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
    // When a request is to be inserted, start from the end as the request's
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

int32_t RowRequestMgr::InformReply(int32_t table_id, int32_t row_id,
    int32_t clock, uint32_t curr_version, std::vector<int32_t> *app_thread_ids) {
  (*app_thread_ids).clear();
  std::pair<int32_t, int32_t> request_key(table_id, row_id);
  std::list<RowRequestInfo> &request_list = pending_row_requests_[request_key];
  int32_t clock_to_request = -1;

  //VLOG(0) << "request_list.empty() = " << request_list.empty();

  while (!request_list.empty()) {
    RowRequestInfo &request = request_list.front();
    //VLOG(0) << "Get request, clock = " << request.clock;
    if (request.clock <= clock) {
      // remove the request
      request_list.pop_front();
      app_thread_ids->push_back(request.app_thread_id);
      uint32_t req_version = request.version;
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

bool RowRequestMgr::AddOpLog(uint32_t version, BgOpLog *oplog) {
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

BgOpLog *RowRequestMgr::GetOpLog(uint32_t version) {
  auto iter = version_oplog_map_.find(version);
  CHECK(iter != version_oplog_map_.end());
  return iter->second;
}

void RowRequestMgr::CleanVersionOpLogs(uint32_t req_version,
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

}  // namespace petuum
