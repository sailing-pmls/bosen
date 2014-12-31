/*
 *  Author: jinlianw
 */

#ifndef __PETUUM_SERVER_PROXY__
#define __PETUUM_SERVER_PROXY__


#include "petuum_ps/comm_handler/comm_handler.hpp"
#include "petuum_ps/server/server_mt.hpp"
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
    /*=========== Static Funcitons ===========*/
    // Entry point of server threads
    static void *ThreadMain(void *_info);

    // ServerProxy assumes that comm_handler has already connected
    // to all clients.
    // TODO(Jinliang): move CommHandler GetConnection inside this class
    ServerProxy(CommHandler *_comm);

    // _num_nonnamenode_sers are the number of servers expected
    // _num_clis are the number of clients expected
    // vectors do not have to be properly sized
    int InitNameNode(int32_t _num_nonnamenode_sers, int32_t _num_clis);

    // vectors do not have to be properly sized
    int InitNonNameNode(int32_t _nn_id, std::string _ip, std::string _port,
        int32_t _num_clis);

  private:
    CommHandler *comm_;

    bool is_namenode_;
    std::vector<int32_t> server_ids_;
    std::vector<int32_t> client_ids_;

    Server<V> server_logic_;

    pthread_mutex_t mtx_;

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
ServerProxy<V>::ServerProxy(CommHandler *_comm):
  comm_(_comm),
  mtx_(PTHREAD_MUTEX_INITIALIZER){}

  template<typename V>
int ServerProxy<V>::InitNameNode(int32_t _num_nonnamenode_sers, int32_t _num_clis){

  is_namenode_ = true;

  server_ids_.resize(_num_nonnamenode_sers);
  client_ids_.resize(_num_clis);

  for(int32_t i = 0; i < _num_nonnamenode_sers; ++i){
    server_ids_[i] = comm_->GetOneConnection();
    assert(server_ids_[i] >= 0);
    VLOG(0) << "*****Get connection from server sid = " << server_ids_[i];
  }

  std::cout << "====================================\n"
    << "Servers are ready to accept clients!\n"
    << "====================================" << std::endl;

  for(int32_t i = 0; i < _num_clis; ++i){
    client_ids_[i] = comm_->GetOneConnection();
    assert(client_ids_[i] >= 0);
    VLOG(0) << "******Get connection from client cid = " << client_ids_[i];
  }

  server_logic_.Init(client_ids_, true);
  return 0;
}

template<typename V>
int ServerProxy<V>::InitNonNameNode(int32_t _nn_id, std::string _ip, 
    std::string _port,
    int32_t _num_clis){
  is_namenode_ = false;
  client_ids_.resize(_num_clis);

  int ret = comm_->ConnectTo(_ip, _port, _nn_id);

  if(ret < 0) return -1;

  for(int32_t i = 0; i < _num_clis; ++i){
    client_ids_[i] = comm_->GetOneConnection();
    assert(client_ids_[i] >= 0);
    VLOG(0) << "******Get connection from client cid = " << client_ids_[i];
  }

  server_logic_.Init(client_ids_, false);
  return 0;
}

template<typename V>
int ServerProxy<V>::BroadcastCreateTable(int32_t _table_id, 
    const TableConfig &_table_config){
  //TODO(jinliang): use the publish pattern

  CHECK(is_namenode_) << "Only namenode may broadcast CreateTable request.";

  SSCreateTableMsg msg(pthread_self(), _table_id, _table_config);

  for(int32_t i = 0; i < server_ids_.size(); ++i){
    int suc = comm_->Send(server_ids_[i], (uint8_t *) &msg, 
        sizeof(SSCreateTableMsg));
    CHECK_EQ(suc, sizeof(SSCreateTableMsg));
    VLOG(0) << "send SSCreateTableMsg to server id = " << server_ids_[i]
      << " my thread_id = " << msg.thrid_
      << " pthread_t = " << pthread_self();
  }

  for(int32_t i = 0; i < server_ids_.size(); ++i){
    boost::shared_array<uint8_t> data;
    int32_t cid;
    int suc = comm_->RecvThr(cid, data);
    CHECK_EQ(suc, sizeof(SSCreateTableReplyMsg));
    SSCreateTableReplyMsg *reply = (SSCreateTableReplyMsg *) data.get();
    CHECK_EQ(TableCreated, reply->ret_);
  }

  return 0;
}

template<typename V>
void *ServerProxy<V>::ThreadMain(void *_info){
  VLOG(0) << "SERVER thread starts";
  ServerThreadInfo<V> *sthr_info = (ServerThreadInfo<V> *) _info;
  ServerProxy<V> *proxy = sthr_info->server_proxy_;

  CommHandler *comm = proxy->comm_;
  Server<V> *server = &(proxy->server_logic_);
  pthread_mutex_t *mtx = &(proxy->mtx_);
  pthread_barrier_t *barrier = sthr_info->barrier_;

  // first I register myself with CommHandler
  pthread_mutex_lock(mtx);
  int suc = comm->RegisterThr(RegThr_Thr && RegThr_Recv);
  assert(suc == 0);
  pthread_mutex_unlock(mtx);
  VLOG(0) << "RegisterThr() successful";

  pthread_barrier_wait(barrier);
  // At this point all threads are created.

  pthread_barrier_wait(barrier);
  // At this point all threads are initialized to namenode or non-namenode.

  bool is_namenode = proxy->is_namenode_;

  int32_t cid;
  boost::shared_array<uint8_t> data;
  bool more;
  // TODO(Jinliang): How to stop server? (Let scheduler do it).
  while(1){
    suc = comm->Recv(cid, data, &more);
    CHECK_GT(suc, 0) << "Recv suc = " << suc
      << " should be positive";
    SCMsgType msg_type = *((SCMsgType *) data.get());
    switch (msg_type) {
      case CreateTable:
        {
          VLOG(0) << "server_proxy gets CreateTable";
          CHECK_EQ(suc, sizeof(SCCreateTableMsg))
            << "CreateTable msg size error!";
          CHECK(is_namenode)
            << "Only namenode should received CreateTable Request.";
          SCCreateTableMsg *msg = (SCCreateTableMsg *) data.get();
          ServerRetMsg ret_msg = server->CreateTableNameNode(msg->table_id_,
              &(msg->table_config_));

          CreateTableReplyType reply_type = TableExisted;

          if (ret_msg.type_ == BroadcastServersReply) {
            int ret = proxy->BroadcastCreateTable(msg->table_id_,
                msg->table_config_);
            CHECK_EQ(0, ret);
            reply_type = TableCreated;
          }

          // must send thread-specific response back
          SCCreateTableReplyMsg reply(msg->table_id_, reply_type, 
              msg->table_config_);
          int suc = comm->SendThr(cid, msg->thrid_, (uint8_t *) &reply, 
              sizeof(SCCreateTableReplyMsg));
          CHECK_EQ(suc, sizeof(SCCreateTableReplyMsg));
          break;
        }
      case GetRow:
        {
          VLOG(0) << "server_proxy gets GetRow from clien " << cid;
          SCGetRowMsg *msg = (SCGetRowMsg *) data.get();
          CHECK_EQ(suc, sizeof(SCGetRowMsg)) << "message size does not match!";
          boost::shared_array<uint8_t> row_data;
          int32_t num_bytes;
          ServerRetMsg ret_msg = server->GetRow(cid, msg->thrid_, msg->table_id_, 
              msg->row_id_, row_data, num_bytes,
              msg->stalest_iter_);

          SCGetRowReplyMsg reply(msg->table_id_, msg->row_id_, ret_msg.ret_);
          if(ret_msg.type_ == ReplySender){
            int suc = comm->SendThr(cid, msg->thrid_, (uint8_t *) &reply, 
                sizeof(SCGetRowReplyMsg), true, true);

            assert(suc == sizeof(SCGetRowReplyMsg));

            suc = comm->SendThr(cid, msg->thrid_, (uint8_t *) row_data.get(), 
                num_bytes, false, false);
            assert(suc == num_bytes);
            VLOG(0) << "GetRow replied cid = " << cid 
              << " table_id = " << msg->table_id_
              << " row_id = " << msg->row_id_;

          }else if(ret_msg.type_ == DropMsg){
            VLOG(0) << "GetRow droped cid = " << cid 
              << " table_id = " << msg->table_id_
              << " row_id = " << msg->row_id_;
            break; // server is not ready to return that row, block the client
          }else{
            assert(0);
          }
          break;
        }
      case PushOpLog:
        {
          VLOG(0) << "server_proxy gets PushOpLog from client " << cid;
          CHECK_EQ(suc, sizeof(SCPushOpLogMsg)) << "msg size error";
          SCPushOpLogMsg *msg = (SCPushOpLogMsg *) data.get();
          boost::shared_array<uint8_t> bytes;
          int suc = comm->Recv(bytes);
          CHECK(suc > 0) << "OpLog size not positive";
          std::vector<EntryOpExtended<V> > oplogs;
          int deserialize_ret = DeserializeOpLogs(bytes, &oplogs);
          ServerRetMsg ret_msg = server->ApplyOpLogs(msg->table_id_, oplogs);
          assert(ret_msg.type_ == DropMsg);
          // no need to resply
        }
        break;
      case SendIterate:
        {
          VLOG(0) << "server_proxy gets SendIterate from client " << cid;
          CHECK_EQ(suc, sizeof(SCSendIterateMsg)) << "msg size error";
          SCSendIterateMsg *msg = (SCSendIterateMsg *) data.get();
          boost::shared_array<uint8_t> bytes;
          int suc = comm->Recv(bytes);
          CHECK(suc > 0) << "OpLog size not positive";

          std::vector<EntryOpExtended<V> > oplogs;
          int deserialize_ret = DeserializeOpLogs(bytes, &oplogs);
          std::vector<RowReadReply> read_reply;
          ServerRetMsg ret_msg = server->Iterate(cid, msg->table_id_, oplogs,
              &read_reply);
          if(ret_msg.type_ == SendMsgs){
            for(std::vector<RowReadReply>::iterator iter = read_reply.begin();
                iter != read_reply.end(); ++iter){

              for(std::vector<ReadRequest>::iterator req_iter = 
                  iter->requests_.begin(); 
                  req_iter != iter->requests_.end();
                  ++req_iter){

                SCGetRowReplyMsg reply_msg(msg->table_id_, iter->row_id_, 0);
                int suc = comm->SendThr(req_iter->client_id_, 
                    req_iter->thread_id_, 
                    (uint8_t *) &reply_msg, 
                    sizeof(SCGetRowReplyMsg), true, true);

                suc = comm->SendThr(req_iter->client_id_, req_iter->thread_id_, 
                    (uint8_t *) iter->data_.get(),
                    iter->num_bytes_, false, false);

                assert(suc == iter->num_bytes_);

                VLOG(0) << "GetRow supplied cid = " << cid 
                  << " table_id = " << msg->table_id_;

              }
            }
          }
        }
        break;
      case NNCreateTable:
        {
          // Server-to-server CreateTable request.
          VLOG(0) << "server_proxy gets NNCreateTable";
          CHECK_EQ(suc, sizeof(SSCreateTableMsg));
          CHECK(!is_namenode)
            << "Only non-namenode should received NNCreateTable Request.";
          SSCreateTableMsg *msg = (SSCreateTableMsg *) data.get();
          ServerRetMsg ret_msg = server->CreateTableNonNameNode(
              msg->table_id_, &(msg->table_config_));
          CHECK_EQ(ReplySender, ret_msg.type_);
          CHECK_EQ(0,ret_msg.ret_);

          // must send thread-specific response back
          SSCreateTableReplyMsg reply(msg->table_id_, TableCreated);
          int suc = comm->SendThr(cid, msg->thrid_, (uint8_t *) &reply, 
              sizeof(SSCreateTableReplyMsg));
          assert(suc == sizeof(SSCreateTableReplyMsg));
          VLOG(0) << "Sent SSCreateTableReplyMsg to cid = " << cid
            << " thread_id = " << msg->thrid_;
          break;
        }
      default:
        VLOG(0) << "default ERROR";
        LOG(FATAL) << "Unrecoganized command = " << msg_type
          << " msg_size = " << suc
          << " FYI, CreateTable = " << CreateTable
          << " GetRow = " << GetRow
          << " PusOpLog = " << SendIterate
          << " NNCreateTable = " << NNCreateTable
          << " NNCreateTableReply = " << NNCreateTableReply
          << " Init = " << Init
          << " InitReply = " << InitReply;
    }
  }
  pthread_mutex_lock(mtx);
  comm->DeregisterThr(RegThr_Thr && RegThr_Recv);
  pthread_mutex_unlock(mtx);
}

}  // namespace petuum

#endif  // __PETUUM_SERVER_PROXY__
