/*
 *  Author: jinlianw
 */

#ifndef __PETUUM_SERVER_PROXY__
#define __PETUUM_SERVER_PROXY__

#include "petuum_ps/comm_handler/comm_handler.hpp"
#include "petuum_ps/server/server.hpp"
#include "petuum_ps/include/configs.hpp"
#include "petuum_ps/proxy/protocol.hpp"
#include "petuum_ps/consistency/op_log_manager.hpp"
#include "petuum_ps/storage/dense_row.hpp"

#include <iostream>
#include <boost/shared_array.hpp>
#include <boost/utility.hpp>
#include <glog/logging.h>
#include <vector>
#include <string>
#include <pthread.h>
#include <assert.h>

namespace petuum {

template<typename V>
class ServerProxy;

template<typename V>
struct ServerThreadInfo{
  ServerProxy<V> *server_proxy_;
  pthread_barrier_t *barrier_;
};

template<typename V>
class ServerProxy : boost::noncopyable {
  public:
    // ServerProxy assumes that comm_handler has already connected
    // to all clients.
    // TODO(Jinliang): move CommHandler GetConnection inside this class
    ServerProxy(CommHandler *_comm);

    // Start the proxy. Single thread only.
    void StartProxy();

    // _num_nonnamenode_sers are the number of servers expected
    // _num_clis are the number of clients expected
    // vectors do not have to be properly sized
    int InitNameNode(int32_t _num_nonnamenode_sers, int32_t _num_clis);

    // vectors do not have to be properly sized
    int InitNonNameNode(int32_t server_id, const std::string& namenode_ip,
        const std::string& namenode_port, int32_t _num_clis);

  private:
    CommHandler *comm_;

    bool is_namenode_;
    std::vector<int32_t> server_ids_;
    std::vector<int32_t> client_ids_;

    Server<V> server_logic_;

    // A blocking "RPC" call.
    // Namenode broadcasts CreateTable message to all servers given.
    // Does not return until it receives responses from all servers.
    // Other servers should *NEVER* fail in creating tables.
    // return 0 for success, negative for failure.
    // Success means all servers created table successfully.
    int BroadcastCreateTable(int32_t _table_id,
        const TableConfig &_table_config);
};

// =============== ServerProxy Implementation ===============

template<typename V>
ServerProxy<V>::ServerProxy(CommHandler *_comm) : comm_(_comm) { }

template<typename V>
int ServerProxy<V>::InitNameNode(int32_t _num_nonnamenode_sers,
    int32_t _num_clis) {
  is_namenode_ = true;

  // Register with CommHandler
  CHECK_EQ(0, comm_->RegisterThr(RegThr_Thr && RegThr_Recv));
  VLOG(0) << "RegisterThr() successful";

  server_ids_.resize(_num_nonnamenode_sers);
  client_ids_.resize(_num_clis);

  // Wait for connections from non-namenode servers.
  for(int32_t i = 0; i < _num_nonnamenode_sers; ++i){
    server_ids_[i] = comm_->GetOneConnection();
    CHECK_GE(server_ids_[i], 0);
    VLOG(0) << "*****Get connection from server sid = " << server_ids_[i];
  }

  std::cout << "====================================\n"
    << "Servers are ready to accept clients!\n"
    << "====================================" << std::endl;

  // Wait for connections from clients.
  for(int32_t i = 0; i < _num_clis; ++i){
    client_ids_[i] = comm_->GetOneConnection();
    CHECK_GE(client_ids_[i], 0);
    VLOG(0) << "******Get connection from client client_id = " << client_ids_[i];
  }

  server_logic_.Init(client_ids_, is_namenode_);
  return 0;
}

template<typename V>
int ServerProxy<V>::InitNonNameNode(int32_t server_id,
    const std::string& namenode_ip, const std::string& namenode_port,
    int32_t _num_clis) {
  is_namenode_ = false;
  client_ids_.resize(_num_clis);

  // Register with CommHandler
  CHECK_EQ(0, comm_->RegisterThr(RegThr_Thr && RegThr_Recv));
  VLOG(0) << "RegisterThr() successful";

  // Initiate connection to namenode.
  int ret = comm_->ConnectTo(namenode_ip, namenode_port, server_id);

  if (ret < 0) return -1;

  // Wait for connections from clients.
  for (int32_t i = 0; i < _num_clis; ++i){
    client_ids_[i] = comm_->GetOneConnection();
    CHECK_GE(client_ids_[i], 0);
    VLOG(0) << "******Get connection from client client_id = " << client_ids_[i];
  }

  server_logic_.Init(client_ids_, is_namenode_);
  return 0;
}

template<typename V>
int ServerProxy<V>::BroadcastCreateTable(int32_t _table_id,
    const TableConfig &_table_config){
  //TODO(jinliang): use the publish pattern

  CHECK(is_namenode_) << "Only namenode may broadcast CreateTable request.";

  SSCreateTableMsg msg(pthread_self(), _table_id, _table_config);

  for(int32_t i = 0; i < server_ids_.size(); ++i){
    int recv_size = comm_->Send(server_ids_[i], (uint8_t *) &msg, 
        sizeof(SSCreateTableMsg));
    CHECK_EQ(recv_size, sizeof(SSCreateTableMsg));
    VLOG(0) << "send SSCreateTableMsg to server id = " << server_ids_[i]
      << " my thread_id = " << msg.thrid_
      << " pthread_t = " << pthread_self();
  }

  for(int32_t i = 0; i < server_ids_.size(); ++i){
    boost::shared_array<uint8_t> data;
    int32_t client_id;
    int recv_size = comm_->RecvThr(client_id, data);
    CHECK_EQ(recv_size, sizeof(SSCreateTableReplyMsg));
    SSCreateTableReplyMsg *reply = (SSCreateTableReplyMsg *) data.get();
    CHECK_EQ(TableCreated, reply->ret_);
  }
  return 0;
}

template<typename V>
void ServerProxy<V>::StartProxy(){
  VLOG(0) << "SERVER thread starts";

  int32_t client_id;
  boost::shared_array<uint8_t> data;
  // TODO(Jinliang): How to stop server? (Let scheduler do it).
  while (true) {
    bool more;
    int recv_size = comm_->Recv(client_id, data, &more);
    CHECK_GT(recv_size, 0) << "Recv recv_size = " << recv_size
      << " should be positive";
    SCMsgType msg_type = *((SCMsgType *) data.get());
    switch (msg_type) {
      case CreateTable:
        {
          VLOG(0) << "server_proxy gets CreateTable";
          CHECK_EQ(sizeof(SCCreateTableMsg), recv_size)
            << "CreateTable msg size error!";
          CHECK(is_namenode_)
            << "Only namenode should received CreateTable Request.";
          SCCreateTableMsg *msg = (SCCreateTableMsg *) data.get();
          ServerRetMsg ret_msg = server_logic_.CreateTableNameNode(msg->table_id_,
              &(msg->table_config_));

          CreateTableReplyType reply_type = TableExisted;

          if (ret_msg.type_ == BroadcastServersReply) {
            int ret = BroadcastCreateTable(msg->table_id_,
                msg->table_config_);
            CHECK_EQ(0, ret);
            reply_type = TableCreated;
          }

          // must send thread-specific response back
          SCCreateTableReplyMsg reply(msg->table_id_, reply_type,
              msg->table_config_);
          int send_size = comm_->SendThr(client_id, msg->thrid_, (uint8_t *) &reply,
              sizeof(SCCreateTableReplyMsg));
          CHECK_EQ(send_size, sizeof(SCCreateTableReplyMsg));
          break;
        }

      case GetRow:
        {
          VLOG(0) << "server_proxy gets GetRow from client " << client_id;
          SCGetRowMsg *msg = (SCGetRowMsg *) data.get();
          CHECK_EQ(sizeof(SCGetRowMsg), recv_size)
            << "message size does not match!";
          boost::shared_array<uint8_t> row_data;
          int32_t num_bytes;
          ServerRetMsg ret_msg = server_logic_.GetRow(client_id, msg->thrid_,
              msg->table_id_, msg->row_id_, row_data, num_bytes,
              msg->stalest_iter_);

          SCGetRowReplyMsg reply(msg->table_id_, msg->row_id_, ret_msg.ret_);
          if (ret_msg.type_ == ReplySender) {
            // Send first part: SCGetRowReplyMsg.
            int send_size =  comm_->SendThr(client_id, msg->thrid_, (uint8_t *) &reply,
                sizeof(SCGetRowReplyMsg), true, true);
            CHECK_EQ(sizeof(SCGetRowReplyMsg), send_size);

            // Send second part: row_data
            send_size = comm_->SendThr(client_id, msg->thrid_, (uint8_t *) row_data.get(),
                num_bytes, false, false);
            CHECK_EQ(num_bytes, send_size);

            VLOG(0) << "GetRow replied client_id = " << client_id
              << " table_id = " << msg->table_id_
              << " row_id = " << msg->row_id_;
          } else if (ret_msg.type_ == DropMsg) {
            // server is not ready to return that row, block the client
            VLOG(0) << "GetRow droped client_id = " << client_id
              << " table_id = " << msg->table_id_
              << " row_id = " << msg->row_id_;
          } else {
            LOG(FATAL) << "Unrecognized return message from server.";
          }
          break;
        }

      case PushOpLog:
        {
          // Convert to SCPushOpLogMsg
          VLOG(0) << "server_proxy gets PushOpLog from client " << client_id;
          CHECK_EQ(sizeof(SCPushOpLogMsg), recv_size) << "msg size error";
          SCPushOpLogMsg *msg = (SCPushOpLogMsg *) data.get();

          // Receive the second part which is oplogs.
          boost::shared_array<uint8_t> bytes;
          int op_log_size = comm_->Recv(bytes);
          CHECK_GT(op_log_size, 0) << "OpLog size not positive";

          // Deserialize oplog and apply them.
          std::vector<EntryOpExtended<V> > oplogs;
          int deserialize_ret = DeserializeOpLogs(bytes, &oplogs);
          ServerRetMsg ret_msg =
            server_logic_.ApplyOpLogs(msg->table_id_, oplogs);

          // PushOpLog never replies.
          CHECK_EQ(DropMsg, ret_msg.type_);
          break;
        }

      case SendIterate:
        {
          // Convert to SCSendIterateMsg
          VLOG(0) << "server_proxy gets SendIterate from client " << client_id;
          CHECK_EQ(sizeof(SCSendIterateMsg), recv_size) << "msg size error";
          SCSendIterateMsg *msg = (SCSendIterateMsg *) data.get();

          // Receive second part which is oplogs.
          boost::shared_array<uint8_t> bytes;
          int oplog_size = comm_->Recv(bytes);
          CHECK_GT(oplog_size, 0) << "OpLog size not positive";

          // Deserialize oplogs.
          std::vector<EntryOpExtended<V> > oplogs;
          int deserialize_ret = DeserializeOpLogs(bytes, &oplogs);

          // Iterate the server.
          std::vector<RowReadReply> read_replies;
          ServerRetMsg ret_msg = server_logic_.Iterate(client_id, msg->table_id_,
              oplogs, &read_replies);

          if (ret_msg.type_ == SendMsgs) {
            // Server clock has incremented. Send out pending read requests.
            for (RowReadReply& row_read_reply : read_replies) {
              int row_id = row_read_reply.row_id_;
              for (ReadRequest& read_request : row_read_reply.requests_) {
                SCGetRowReplyMsg reply_msg(msg->table_id_, row_id, 0);
                int send_size = comm_->SendThr(read_request.client_id_,
                    read_request.thread_id_, (uint8_t *) &reply_msg,
                    sizeof(SCGetRowReplyMsg), true, true);
                CHECK_EQ(sizeof(SCGetRowReplyMsg), send_size);

                send_size = comm_->SendThr(read_request.client_id_,
                    read_request.thread_id_,
                    (uint8_t *) row_read_reply.data_.get(),
                    row_read_reply.num_bytes_, false, false);
                CHECK_EQ(row_read_reply.num_bytes_, send_size);

                VLOG(0) << "GetRow supplied client_id = " << client_id 
                  << " table_id = " << msg->table_id_;
              }
            }
          }
          break;
        }

      case NNCreateTable:   // NonNamenode CreateTable
        {
          // Server-to-server CreateTable request.
          VLOG(0) << "server_proxy gets NNCreateTable";
          CHECK_EQ(recv_size, sizeof(SSCreateTableMsg));
          CHECK(!is_namenode_)
            << "Only non-namenode should received NNCreateTable Request.";
          SSCreateTableMsg *msg = (SSCreateTableMsg *) data.get();
          ServerRetMsg ret_msg = server_logic_.CreateTableNonNameNode(
              msg->table_id_, &(msg->table_config_));
          CHECK_EQ(ReplySender, ret_msg.type_);
          CHECK_EQ(0,ret_msg.ret_);

          // must send thread-specific response back
          SSCreateTableReplyMsg reply(msg->table_id_, TableCreated);
          int send_size = comm_->SendThr(client_id, msg->thrid_,
              (uint8_t *) &reply, sizeof(SSCreateTableReplyMsg));
          CHECK_EQ(sizeof(SSCreateTableReplyMsg), send_size);
          VLOG(0) << "Sent SSCreateTableReplyMsg to client_id = " << client_id
            << " thread_id = " << msg->thrid_;
          break;
        }

      default:
        LOG(FATAL) << "Unrecoganized command = " << msg_type
          << " msg_size = " << recv_size
          << " FYI, CreateTable = " << CreateTable
          << " GetRow = " << GetRow
          << " PusOpLog = " << SendIterate
          << " NNCreateTable = " << NNCreateTable
          << " NNCreateTableReply = " << NNCreateTableReply
          << " Init = " << Init
          << " InitReply = " << InitReply;
    }
  }
  comm_->DeregisterThr(RegThr_Thr && RegThr_Recv);
}

}  // namespace petuum

#endif  // __PETUUM_SERVER_PROXY__
