// namenode_threads.hpp
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

class NameNodeThread {
public:
  static void Init();
  static void ShutDown();

private:
  struct CreateTableInfo {
    int32_t num_clients_replied_;
    int32_t num_servers_replied_;
    std::queue<int32_t> bgs_to_reply_;
    CreateTableInfo():
      num_clients_replied_(0),
      num_servers_replied_(0),
      bgs_to_reply_(){}

    ~CreateTableInfo(){}

    CreateTableInfo & operator= (const CreateTableInfo& info_obj){
      num_clients_replied_ = info_obj.num_clients_replied_;
      num_servers_replied_ = info_obj.num_servers_replied_;
      bgs_to_reply_ = info_obj.bgs_to_reply_;
      return *this;
    }

    bool ReceivedFromAllServers() const {
      return (num_servers_replied_ == GlobalContext::get_num_servers());
    }

    bool RepliedToAllClients() const {
      return (num_clients_replied_ == GlobalContext::get_num_clients());
    }
  };

  // server context is specific to the server thread
  struct NameNodeContext {
    std::vector<int32_t> bg_thread_ids_;
    // one bg per client is refered to as head bg
    std::map<int32_t, CreateTableInfo> create_table_map_;
    Server server_obj_;

    int32_t num_shutdown_bgs_;
  };

  static void *NameNodeThreadMain(void *server_thread_info);

  // communication function
  static int32_t GetConnection(bool *is_client, int32_t *client_id);
  static void SendToAllServers(void *msg, size_t msg_size);

  /**
   * Functions that operate on the particular thread's specific NameNodeContext.
   */
  static void SetUpNameNodeContext();
  static void SetUpCommBus();
  static void InitNameNode();

  static bool HaveCreatedAllTables();
  static void SendCreatedAllTablesMsg();

  static void SendToAllBgThreads(void *msg, size_t msg_size);
  static bool HandleShutDownMsg(); // returns true if the server may shut down
  static void HandleCreateTable(int32_t sender_id,
    CreateTableMsg &create_table_msg);
  static void HandleCreateTableReply(
    CreateTableReplyMsg &create_table_reply_msg);

  static pthread_barrier_t init_barrier;
  static pthread_t thread_;
  static boost::thread_specific_ptr<NameNodeContext> name_node_context_;

  static CommBus::RecvFunc CommBusRecvAny;
  static CommBus::RecvTimeOutFunc CommBusRecvTimeOutAny;
  static CommBus::SendFunc CommBusSendAny;

  static CommBus *comm_bus_;
};
}
