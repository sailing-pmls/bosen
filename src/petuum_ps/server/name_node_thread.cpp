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
// server_thread.cpp
// author: jinliang

#include "petuum_ps/server/name_node_thread.hpp"
#include "petuum_ps/thread/context.hpp"
#include "petuum_ps/thread/ps_msgs.hpp"

#include <iostream>

namespace petuum {

pthread_barrier_t NameNodeThread::init_barrier;
pthread_t NameNodeThread::thread_;
boost::thread_specific_ptr<NameNodeThread::NameNodeContext>
  NameNodeThread::name_node_context_;
CommBus::RecvFunc NameNodeThread::CommBusRecvAny;
CommBus::RecvTimeOutFunc NameNodeThread::CommBusRecvTimeOutAny;
CommBus::SendFunc NameNodeThread::CommBusSendAny;
CommBus *NameNodeThread::comm_bus_;

void NameNodeThread::Init(){
  pthread_barrier_init(&init_barrier, NULL, 2);
  comm_bus_ = GlobalContext::comm_bus;

  if (GlobalContext::get_num_clients() == 1) {
    CommBusRecvAny = &CommBus::RecvInProc;
  } else{
    CommBusRecvAny = &CommBus::Recv;
  }

  if (GlobalContext::get_num_clients() == 1) {
    CommBusRecvTimeOutAny = &CommBus::RecvInProcTimeOut;
  } else{
    CommBusRecvTimeOutAny = &CommBus::RecvTimeOut;
  }

  if (GlobalContext::get_num_clients() == 1) {
    CommBusSendAny = &CommBus::SendInProc;
  } else {
    CommBusSendAny = &CommBus::Send;
  }

  int32_t name_node_id = GlobalContext::get_name_node_id();

  int ret = pthread_create(&thread_, NULL, NameNodeThreadMain, &name_node_id);
  CHECK_EQ(ret, 0);

  pthread_barrier_wait(&init_barrier);
}

void NameNodeThread::ShutDown() {
  int ret = pthread_join(thread_, NULL);
  CHECK_EQ(ret, 0);
}

/* Private Functions */

void NameNodeThread::SendToAllBgThreads(void *msg, size_t msg_size){
  int32_t i;
  for(i = 0; i < GlobalContext::get_num_total_bg_threads(); ++i){
    int32_t bg_id = name_node_context_->bg_thread_ids_[i];
    size_t sent_size = (comm_bus_->*CommBusSendAny)(bg_id, msg, msg_size);
    CHECK_EQ(sent_size, msg_size);
  }
}

void NameNodeThread::SendToAllServers(void *msg, size_t msg_size){
  int32_t i;
  std::vector<int32_t> server_ids = GlobalContext::get_server_ids();
  for(i = 0; i < GlobalContext::get_num_servers(); ++i){
    int32_t server_id = server_ids[i];
    size_t sent_size = (comm_bus_->*CommBusSendAny)(server_id, msg, msg_size);
    CHECK_EQ(sent_size, msg_size);
  }
}

int32_t NameNodeThread::GetConnection(bool *is_client, int32_t *client_id){
  int32_t sender_id;
  zmq::message_t zmq_msg;
  (comm_bus_->*CommBusRecvAny)(&sender_id, &zmq_msg);
  MsgType msg_type = MsgBase::get_msg_type(zmq_msg.data());
  if(msg_type == kClientConnect){
    ClientConnectMsg msg(zmq_msg.data());
    *is_client = true;
    *client_id = msg.get_client_id();
  }else{
    CHECK_EQ(msg_type, kServerConnect);
    *is_client = false;
  }
  return sender_id;
}

void NameNodeThread::InitNameNode() {
  int32_t num_bgs = 0;
  int32_t num_servers = 0;
  int32_t num_expected_conns = GlobalContext::get_num_total_bg_threads()
    + GlobalContext::get_num_servers();
  VLOG(0) << "Number total_bg_threads() = "
	  << GlobalContext::get_num_total_bg_threads();
  VLOG(0) << "Number total_server_threads() = "
	  << GlobalContext::get_num_servers();
  for (int32_t num_connections = 0; num_connections < num_expected_conns;
    ++num_connections) {
    int32_t client_id;
    bool is_client;
    int32_t sender_id = GetConnection(&is_client, &client_id);
    if (is_client) {
      name_node_context_->bg_thread_ids_[num_bgs] = sender_id;
      ++num_bgs;
      name_node_context_->server_obj_.AddClientBgPair(client_id, sender_id);
      VLOG(0) << "Name node gets client " << sender_id;
    }else{
      ++num_servers;
      VLOG(0) << "Name node gets server " << sender_id;
    }
  }

  CHECK_EQ(num_bgs, GlobalContext::get_num_total_bg_threads());
  name_node_context_->server_obj_.Init();

  VLOG(0) << "Sending out connect_server_msg";
  ConnectServerMsg connect_server_msg;
  int32_t msg_size = connect_server_msg.get_size();
  SendToAllBgThreads(connect_server_msg.get_mem(), msg_size);

  VLOG(0) << "Sent out connect_server_msg";

  ClientStartMsg client_start_msg;
  msg_size = client_start_msg.get_size();
  SendToAllBgThreads(client_start_msg.get_mem(), msg_size);

  VLOG(0) << "InitNameNode done";
}

bool NameNodeThread::HaveCreatedAllTables(){
  const std::map<int32_t, CreateTableInfo>& create_table_map
    = name_node_context_->create_table_map_;
  if((int32_t) create_table_map.size() < GlobalContext::get_num_tables())
    return false;

  for(auto iter = create_table_map.cbegin();
    iter != create_table_map.cend(); iter++){
    if(!iter->second.RepliedToAllClients()){
      return false;
    }
  }
  return true;
}

void NameNodeThread::SendCreatedAllTablesMsg() {
  const std::map<int32_t, bool> &head_bgs = name_node_context_->head_bgs_;

  CreatedAllTablesMsg created_all_tables_msg;
  int32_t num_clients = head_bgs.size();
  CHECK_EQ(num_clients, GlobalContext::get_num_clients());

  for(auto iter = head_bgs.cbegin(); iter != head_bgs.cend(); iter++){
    size_t sent_size = (comm_bus_->*CommBusSendAny)(iter->first,
      created_all_tables_msg.get_mem(), created_all_tables_msg.get_size());

    CHECK_EQ(sent_size, created_all_tables_msg.get_size());
  }
}

bool NameNodeThread::HandleShutDownMsg(){
  // When num_shutdown_bgs reaches the total number of bg threads, the server
  // reply to each bg with a ShutDownReply message
  int32_t &num_shutdown_bgs = name_node_context_->num_shutdown_bgs_;
  ++num_shutdown_bgs;
  if(num_shutdown_bgs == GlobalContext::get_num_total_bg_threads()){
    ServerShutDownAckMsg shut_down_ack_msg;
    size_t msg_size = shut_down_ack_msg.get_size();
    int i;
    for(i = 0; i < GlobalContext::get_num_total_bg_threads(); ++i){
      int32_t bg_id = name_node_context_->bg_thread_ids_[i];
      size_t sent_size = (comm_bus_->*CommBusSendAny)(bg_id,
        shut_down_ack_msg.get_mem(), msg_size);
      CHECK_EQ(msg_size, sent_size);
    }
    return true;
  }
  return false;
}

void NameNodeThread::HandleCreateTable(int32_t sender_id,
  CreateTableMsg &create_table_msg) {
  int32_t table_id = create_table_msg.get_table_id();
  // if name node, send to all other servers; otherwise, just reply
  std::map<int32_t, CreateTableInfo> &create_table_map =
    name_node_context_->create_table_map_;
  std::map<int32_t, bool> &head_bgs = name_node_context_->head_bgs_;
  if (create_table_map.count(table_id) == 0) {
    TableInfo table_info;
    table_info.table_staleness = create_table_msg.get_staleness();
    table_info.row_type = create_table_msg.get_row_type();
    table_info.row_capacity = create_table_msg.get_row_capacity();
    name_node_context_->server_obj_.CreateTable(table_id, table_info);

    create_table_map[table_id]; // access it to call default constructor
    SendToAllServers(create_table_msg.get_mem(), create_table_msg.get_size());
  }
  head_bgs[sender_id] = true;
  if (create_table_map[table_id].ReceivedFromAllServers()) {
    CreateTableReplyMsg create_table_reply_msg;
    create_table_reply_msg.get_table_id() = create_table_msg.get_table_id();
    size_t sent_size = (comm_bus_->*CommBusSendAny)(sender_id,
      create_table_reply_msg.get_mem(), create_table_reply_msg.get_size());
    CHECK_EQ(sent_size, create_table_reply_msg.get_size());
    ++create_table_map[table_id].num_clients_replied_;
    if (HaveCreatedAllTables())
      SendCreatedAllTablesMsg();
  } else {
    // to be sent later
    create_table_map[table_id].bgs_to_reply_.push(sender_id);
  }
}

void NameNodeThread::HandleCreateTableReply(
  CreateTableReplyMsg &create_table_reply_msg){

  int32_t table_id = create_table_reply_msg.get_table_id();
  std::map<int32_t, CreateTableInfo> &create_table_map =
    name_node_context_->create_table_map_;

  ++create_table_map[table_id].num_servers_replied_;
  if (create_table_map[table_id].ReceivedFromAllServers()) {
    std::queue<int32_t> &bgs_to_reply
      = create_table_map[table_id].bgs_to_reply_;
    while (!bgs_to_reply.empty()) {
      int32_t bg_id = bgs_to_reply.front();
      bgs_to_reply.pop();
      size_t sent_size = (comm_bus_->*CommBusSendAny)(bg_id,
        create_table_reply_msg.get_mem(), create_table_reply_msg.get_size());
      CHECK_EQ(sent_size, create_table_reply_msg.get_size());
      ++create_table_map[table_id].num_clients_replied_;
    }
    if (HaveCreatedAllTables())
      SendCreatedAllTablesMsg();
  }
}

void NameNodeThread::SetUpNameNodeContext(){
  name_node_context_.reset(new NameNodeContext);
  name_node_context_->bg_thread_ids_.resize(
    GlobalContext::get_num_total_bg_threads());
  name_node_context_->num_shutdown_bgs_ = 0;
}

void NameNodeThread::SetUpCommBus() {
  int32_t my_id = ThreadContext::get_id();
  CommBus::Config comm_config;
  comm_config.entity_id_ = my_id;

  if (GlobalContext::get_num_clients() > 1) {
    comm_config.ltype_ = CommBus::kInProc | CommBus::kInterProc;
    HostInfo host_info = GlobalContext::get_host_info(my_id);
    comm_config.network_addr_ = host_info.ip + ":" + host_info.port;
  } else {
    comm_config.ltype_ = CommBus::kInProc;
  }

  comm_bus_->ThreadRegister(comm_config);
  std::cout << "NameNode is ready to accept connections!" << std::endl;
}

void *NameNodeThread::NameNodeThreadMain(void *thread_id){
  int32_t my_id = *(reinterpret_cast<int32_t*>(thread_id));

  ThreadContext::RegisterThread(my_id);

  // set up thread-specific server context
  SetUpNameNodeContext();
  SetUpCommBus();

  pthread_barrier_wait(&init_barrier);

  InitNameNode();
  zmq::message_t zmq_msg;
  int32_t sender_id;
  while(1) {
    (comm_bus_->*CommBusRecvAny)(&sender_id, &zmq_msg);
    MsgType msg_type = MsgBase::get_msg_type(zmq_msg.data());
    VLOG(0) << "msg_type = " << msg_type;

    switch (msg_type) {
    case kClientShutDown:
      {
	VLOG(0) << "get ClientShutDown from bg " << sender_id;
	bool shutdown = HandleShutDownMsg();
	if(shutdown){
	  VLOG(0) << "NameNode shutting down";
	  comm_bus_->ThreadDeregister();
	  return 0;
	}
	break;
      }
    case kCreateTable:
      {
	CreateTableMsg create_table_msg(zmq_msg.data());
	HandleCreateTable(sender_id, create_table_msg);
	break;
      }
    case kCreateTableReply:
      {
	CreateTableReplyMsg create_table_reply_msg(zmq_msg.data());
	HandleCreateTableReply(create_table_reply_msg);
	break;
      }
    default:
      LOG(FATAL) << "Unrecognized message type " << msg_type
		 << " sender = " << sender_id;
    }
  }
}

}
