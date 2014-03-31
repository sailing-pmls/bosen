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
// server_threads.hpp
// author: jinliang

#pragma once

#include <vector>
#include <pthread.h>
#include <boost/thread/tss.hpp>
#include <queue>

#include "petuum_ps/server/server.hpp"
#include "petuum_ps/thread/ps_msgs.hpp"
#include "petuum_ps/thread/context.hpp"
#include "petuum_ps/comm_bus/comm_bus.hpp"

namespace petuum {

class ServerThreads {
public:
  static void Init(int32_t id_st);
  static void ShutDown();

private:
  // server context is specific to the server thread
  struct ServerContext {
    std::vector<int32_t> bg_thread_ids_;
    // one bg per client is refered to as head bg
    std::map<int32_t, bool> head_bgs_;
    Server server_obj_;
    int32_t num_shutdown_bgs_;
  };

  static void *ServerThreadMain(void *server_thread_info);

  // communication function
  // assuming the caller is not name node
  static void ConnectToNameNode();
  static int32_t GetConnection(bool *is_client, int32_t *client_id);
  
  /**
   * Functions that operate on the particular thread's specific ServerContext.
   */
  static void SetUpServerContext();
  static void SetUpCommBus();
  static void InitServer();

  static void SendToAllBgThreads(void *msg, size_t msg_size);
  static bool HandleShutDownMsg(); // returns true if the server may shut down
  static void HandleCreateTable(int32_t sender_id,
    CreateTableMsg &create_table_msg);
  static void HandleRowRequest(int32_t sender_id,
    RowRequestMsg &row_request_msg);
  static void ReplyRowRequest(int32_t bg_id, ServerRow *server_row,
    int32_t table_id, int32_t row_id, int32_t server_clock, uint32_t version);
  static void HandleOpLogMsg(int32_t sender_id,
    ClientSendOpLogMsg &client_send_oplog_msg);

  static pthread_barrier_t init_barrier;
  static std::vector<pthread_t> threads_;
  static std::vector<int32_t> thread_ids_;
  static boost::thread_specific_ptr<ServerContext> server_context_;

  static CommBus::RecvFunc CommBusRecvAny;
  static CommBus::RecvTimeOutFunc CommBusRecvTimeOutAny;
  static CommBus::SendFunc CommBusSendAny;

  static CommBus *comm_bus_;
};
}
