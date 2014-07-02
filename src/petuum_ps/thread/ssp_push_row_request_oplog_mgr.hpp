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

#pragma once

#include <map>
#include <vector>
#include <list>
#include <utility>
#include <boost/noncopyable.hpp>
#include <glog/logging.h>

#include <petuum_ps/thread/bg_oplog.hpp>
#include <petuum_ps/thread/row_request_oplog_mgr.hpp>
#include <petuum_ps/thread/server_version_mgr.hpp>

namespace petuum {

// Managing row requests and OpLogs for SSPPush mode.
// Use the same logic as SSPRowRequestOpLogMgr to avoid repeated row requests
// to server.
// Row requests management is completely separated from oplog management.
// A BgOpLog is always saved.
// When the bg worker receives a set of server-pushed rows, it removes
// OpLogs based on the version number in that msg.

class SSPPushRowRequestOpLogMgr : public RowRequestOpLogMgr {
public:
  SSPPushRowRequestOpLogMgr() :
      server_version_mgr_(GlobalContext::get_server_ids()) { }

  ~SSPPushRowRequestOpLogMgr() {
    for (auto iter = version_oplog_list_.begin();
      iter != version_oplog_list_.end(); iter++) {
      CHECK_NOTNULL(iter->second);
      delete iter->second;
    }
  }

  // return true unless there's a previous request with lower or same clock
  // number
  bool AddRowRequest(RowRequestInfo &request, int32_t table_id, int32_t row_id);

  // Get a list of app thread ids that can be satisfied with this reply.
  // Corresponding row requests are removed upon returning.
  int32_t InformReply(int32_t table_id, int32_t row_id, int32_t clock,
    uint32_t curr_version, std::vector<int32_t> *app_thread_ids);

  // Get OpLog of a particular version.
  BgOpLog *GetOpLog(uint32_t version);

  bool AddOpLog(uint32_t version, BgOpLog *oplog);

  void InformVersionInc();
  // Remove oplogs that are before this version.
  void ServerAcknowledgeVersion(int32_t server_id, uint32_t version);

  BgOpLog *OpLogIterInit(uint32_t start_version, uint32_t end_version);
  BgOpLog *OpLogIterNext(uint32_t *version);

private:
  void CleanVersionOpLogs(uint32_t req_version, uint32_t curr_version);

  // map <table_id, row_id> to a list of requests
  // The list is in increasing order of clock.
  std::map<std::pair<int32_t, int32_t>,
    std::list<RowRequestInfo> > pending_row_requests_;

  // The version number of a request means that all oplogs up to and including
  // this version have been applied to this row.
  std::list<std::pair<uint32_t, BgOpLog*> > version_oplog_list_;
  // used for OpLogIter
  uint32_t oplog_iter_version_next_;
  uint32_t oplog_iter_version_st_;
  uint32_t oplog_iter_version_end_;
  std::list<std::pair<uint32_t, BgOpLog*> >::const_iterator oplog_iter_;

  ServerVersionMgr server_version_mgr_;
};

}  // namespace petuum
