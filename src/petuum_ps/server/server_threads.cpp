// server_thread.cpp
// author: jinliang

#include "petuum_ps/server/server_threads.hpp"
#include "petuum_ps/thread/context.hpp"
#include "petuum_ps/thread/ps_msgs.hpp"
#include "petuum_ps/thread/mem_transfer.hpp"
#include "petuum_ps/util/stats.hpp"

namespace petuum {

pthread_barrier_t ServerThreads::init_barrier;
std::vector<pthread_t> ServerThreads::threads_;
std::vector<int32_t> ServerThreads::thread_ids_;
boost::thread_specific_ptr<ServerThreads::ServerContext>
  ServerThreads::server_context_;
CommBus::RecvFunc ServerThreads::CommBusRecvAny;
CommBus::RecvTimeOutFunc ServerThreads::CommBusRecvTimeOutAny;
CommBus::SendFunc ServerThreads::CommBusSendAny;
CommBus::RecvAsyncFunc ServerThreads::CommBusRecvAsyncAny;
CommBus *ServerThreads::comm_bus_;
ServerThreads::ServerPushRowFunc ServerThreads::ServerPushRow;
CommBus::RecvWrapperFunc ServerThreads::CommBusRecvAnyWrapper;
ServerThreads::RowSubscribeFunc ServerThreads::RowSubscribe;

void ServerThreads::Init(int32_t id_st){

  pthread_barrier_init(&init_barrier, NULL,
    GlobalContext::get_num_local_server_threads() + 1);
  threads_.resize(GlobalContext::get_num_local_server_threads());
  thread_ids_.resize(GlobalContext::get_num_local_server_threads());
  comm_bus_ = GlobalContext::comm_bus;

  if (GlobalContext::get_num_clients() == 1) {
    CommBusRecvAny = &CommBus::RecvInProc;
    CommBusRecvAsyncAny = &CommBus::RecvInProcAsync;
  } else{
    CommBusRecvAny = &CommBus::Recv;
    CommBusRecvAsyncAny = &CommBus::RecvAsync;
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

  //ServerThreadMainFunc ServerThreadMain;
  ConsistencyModel consistency_model = GlobalContext::get_consistency_model();
  switch(consistency_model) {
    case SSP:
      ServerPushRow = SSPServerPushRow;
      RowSubscribe = SSPRowSubscribe;
      break;
    case SSPPush:
      ServerPushRow = SSPPushServerPushRow;
      RowSubscribe = SSPPushRowSubscribe;
      break;
    default:
      LOG(FATAL) << "Unrecognized consistency model " << consistency_model;
  }

  if (GlobalContext::get_aggressive_cpu()) {
    CommBusRecvAnyWrapper = CommBusRecvAnyBusy;
  } else {
    CommBusRecvAnyWrapper = CommBusRecvAnySleep;
  }

  int i;
  for (i = 0; i < GlobalContext::get_num_local_server_threads(); ++i) {
    VLOG(0) << "Create server thread " << i;
    thread_ids_[i] = id_st + i;
    int ret = pthread_create(&threads_[i], NULL, ServerThreadMain,
      &thread_ids_[i]);
    CHECK_EQ(ret, 0);
  }
  pthread_barrier_wait(&init_barrier);
}

void ServerThreads::ShutDown(){
  for(int i = 0; i < GlobalContext::get_num_local_server_threads(); ++i){
    int ret = pthread_join(threads_[i], NULL);
    CHECK_EQ(ret, 0);
  }
}

/* Private Functions */

void ServerThreads::SendToAllBgThreads(void *msg, size_t msg_size){
  int32_t i;
  for(i = 0; i < GlobalContext::get_num_total_bg_threads(); ++i){
    int32_t bg_id = server_context_->bg_thread_ids_[i];
    size_t sent_size = (comm_bus_->*CommBusSendAny)(bg_id, msg, msg_size);
    CHECK_EQ(sent_size, msg_size);
  }
}

void ServerThreads::ConnectToNameNode(){
  int32_t name_node_id = GlobalContext::get_name_node_id();

  ServerConnectMsg server_connect_msg;
  void *msg = server_connect_msg.get_mem();
  int32_t msg_size = server_connect_msg.get_size();

  if (comm_bus_->IsLocalEntity(name_node_id)) {
    VLOG(0) << "Connect to local name node";
    comm_bus_->ConnectTo(name_node_id, msg, msg_size);
  } else {
    VLOG(0) << "Connect to remote name node";
    HostInfo name_node_info = GlobalContext::get_host_info(name_node_id);
    std::string name_node_addr = name_node_info.ip + ":" + name_node_info.port;
    VLOG(0) << "name_node_addr = " << name_node_addr;
    comm_bus_->ConnectTo(name_node_id, name_node_addr, msg, msg_size);
  }
}

int32_t ServerThreads::GetConnection(bool *is_client, int32_t *client_id){
  int32_t sender_id;
  zmq::message_t zmq_msg;
  (comm_bus_->*CommBusRecvAny)(&sender_id, &zmq_msg);
  MsgType msg_type = MsgBase::get_msg_type(zmq_msg.data());
  if (msg_type == kClientConnect) {
    ClientConnectMsg msg(zmq_msg.data());
    *is_client = true;
    *client_id = msg.get_client_id();
  } else {
    CHECK_EQ(msg_type, kServerConnect);
    *is_client = false;
  }
  return sender_id;
}

void ServerThreads::InitServer() {
  VLOG(0) << "Server connect to name node";
  ConnectToNameNode();

  int32_t num_bgs;
  for(num_bgs = 0; num_bgs < GlobalContext::get_num_total_bg_threads();
    ++num_bgs){
    int32_t client_id;
    bool is_client;
    int32_t bg_id = GetConnection(&is_client, &client_id);
    CHECK(is_client);
    server_context_->bg_thread_ids_[num_bgs] = bg_id;
    server_context_->server_obj_.AddClientBgPair(client_id, bg_id);
  }

  server_context_->server_obj_.Init();
  ClientStartMsg client_start_msg;
  int32_t msg_size = client_start_msg.get_size();
  SendToAllBgThreads(client_start_msg.get_mem(), msg_size);

  VLOG(0) << "InitNonNameNode done";
}

bool ServerThreads::HandleShutDownMsg(){
  // When num_shutdown_bgs reaches the total number of bg threads, the server
  // reply to each bg with a ShutDownReply message
  int32_t &num_shutdown_bgs = server_context_->num_shutdown_bgs_;
  ++num_shutdown_bgs;
  if(num_shutdown_bgs == GlobalContext::get_num_total_bg_threads()){
    ServerShutDownAckMsg shut_down_ack_msg;
    size_t msg_size = shut_down_ack_msg.get_size();
    int i;
    for(i = 0; i < GlobalContext::get_num_total_bg_threads(); ++i){
      int32_t bg_id = server_context_->bg_thread_ids_[i];
      size_t sent_size = (comm_bus_->*CommBusSendAny)(bg_id,
        shut_down_ack_msg.get_mem(), msg_size);
      CHECK_EQ(msg_size, sent_size);
    }
    return true;
  }
  return false;
}

void ServerThreads::HandleCreateTable(int32_t sender_id,
  CreateTableMsg &create_table_msg) {
  int32_t table_id = create_table_msg.get_table_id();

  // I'm not name node
  CreateTableReplyMsg create_table_reply_msg;
  create_table_reply_msg.get_table_id() = create_table_msg.get_table_id();
  size_t sent_size = (comm_bus_->*CommBusSendAny)(sender_id,
    create_table_reply_msg.get_mem(), create_table_reply_msg.get_size());
  CHECK_EQ(sent_size, create_table_reply_msg.get_size());

  TableInfo table_info;
  table_info.table_staleness = create_table_msg.get_staleness();
  table_info.row_type = create_table_msg.get_row_type();
  table_info.row_capacity = create_table_msg.get_row_capacity();
  server_context_->server_obj_.CreateTable(table_id, table_info);
}

void ServerThreads::SetUpServerContext(){
  server_context_.reset(new ServerContext);
  server_context_->bg_thread_ids_.resize(
    GlobalContext::get_num_total_bg_threads());
  server_context_->num_shutdown_bgs_ = 0;
}

void ServerThreads::SetUpCommBus() {
  int32_t my_id = ThreadContext::get_id();
  CommBus::Config comm_config;
  comm_config.entity_id_ = my_id;
  VLOG(0) << "ServerThreads num_clients = " << GlobalContext::get_num_clients();
  VLOG(0) << "my id = " << my_id;

  if (GlobalContext::get_num_clients() > 1) {
    comm_config.ltype_ = CommBus::kInProc | CommBus::kInterProc;
    HostInfo host_info = GlobalContext::get_host_info(my_id);
    comm_config.network_addr_ = host_info.ip + ":" + host_info.port;
    VLOG(0) << "network addr = " << comm_config.network_addr_;
  } else {
    comm_config.ltype_ = CommBus::kInProc;
  }

  comm_bus_->ThreadRegister(comm_config);
  VLOG(0) << "Server thread registered CommBus";
}

void ServerThreads::HandleRowRequest(int32_t sender_id,
  RowRequestMsg &row_request_msg) {
  int32_t table_id = row_request_msg.get_table_id();
  int32_t row_id = row_request_msg.get_row_id();
  int32_t clock = row_request_msg.get_clock();
  int32_t server_clock = server_context_->server_obj_.GetMinClock();
  if (server_clock < clock) {
    //VLOG(0) << "server clock = " << server_clock
    //	    << " request clock = " << clock
    //	    << " not fresh enough, should wait";
    server_context_->server_obj_.AddRowRequest(sender_id, table_id, row_id,
      clock);
    return;
  }

  uint32_t version = server_context_->server_obj_.GetBgVersion(sender_id);
  ServerRow *server_row = server_context_->server_obj_.FindCreateRow(table_id,
    row_id);
  RowSubscribe(server_row, GlobalContext::thread_id_to_client_id(sender_id));

  //VLOG(0) << "fresh enough, reply now";
  ReplyRowRequest(sender_id, server_row, table_id, row_id, server_clock,
   version);
}

void ServerThreads::ReplyRowRequest(int32_t bg_id, ServerRow *server_row,
  int32_t table_id, int32_t row_id, int32_t server_clock, uint32_t version){

  size_t row_size = server_row->SerializedSize();

  ServerRowRequestReplyMsg server_row_request_reply_msg(row_size);
  server_row_request_reply_msg.get_table_id() = table_id;
  server_row_request_reply_msg.get_row_id() = row_id;
  server_row_request_reply_msg.get_clock() = server_clock;
  server_row_request_reply_msg.get_version() = version;
  //VLOG(0) << "Replying client row request, version = " << version
  //       << " table_id = " << table_id;

  row_size = server_row->Serialize(server_row_request_reply_msg.get_row_data());

  server_row_request_reply_msg.get_row_size() = row_size;

  /*  size_t sent_size = (comm_bus_->*CommBusSendAny)(bg_id,
    server_row_request_reply_msg.get_mem(),
    server_row_request_reply_msg.get_size());
    CHECK_EQ(sent_size, server_row_request_reply_msg.get_size()); */

  MemTransfer::TransferMem(comm_bus_, bg_id, &server_row_request_reply_msg);
}

void ServerThreads::HandleOpLogMsg(int32_t sender_id,
  ClientSendOpLogMsg &client_send_oplog_msg) {
  int32_t client_id = client_send_oplog_msg.get_client_id();
  bool is_clock = client_send_oplog_msg.get_is_clock();
  uint32_t version = client_send_oplog_msg.get_version();
  server_context_->server_obj_.ApplyOpLog(client_send_oplog_msg.get_data(),
    sender_id, version);

  if (is_clock) {
    bool clock_changed
      = server_context_->server_obj_.Clock(client_id, sender_id);
    if (clock_changed) {
      std::vector<ServerRowRequest> requests;
      server_context_->server_obj_.GetFulfilledRowRequests(&requests);
      for (auto request_iter = requests.begin();
	   request_iter != requests.end(); request_iter++) {
	int32_t table_id = request_iter->table_id;
	int32_t row_id = request_iter->row_id;
	int32_t bg_id = request_iter->bg_id;
	int32_t version = server_context_->server_obj_.GetBgVersion(bg_id);
	ServerRow *server_row
	  = server_context_->server_obj_.FindCreateRow(table_id, row_id);
        RowSubscribe(server_row,
                     GlobalContext::thread_id_to_client_id(bg_id));
	int32_t server_clock = server_context_->server_obj_.GetMinClock();
	ReplyRowRequest(bg_id, server_row, table_id, row_id, server_clock,
	  version);
      }
      ServerPushRow();
    }
  }
}

void ServerThreads::CommBusRecvAnyBusy(int32_t *sender_id,
                                       zmq::message_t *zmq_msg) {
  bool received = (comm_bus_->*CommBusRecvAsyncAny)(sender_id, zmq_msg);
  while (!received) {
    received = (comm_bus_->*CommBusRecvAsyncAny)(sender_id, zmq_msg);
  }
}

void ServerThreads::CommBusRecvAnySleep(int32_t *sender_id,
                                        zmq::message_t *zmq_msg) {
  (comm_bus_->*CommBusRecvAny)(sender_id, zmq_msg);
}

void ServerThreads::SSPPushServerPushRow() {
  VLOG(0) << "SSPPushServerPushRow()";
  server_context_->server_obj_.CreateSendServerPushRowMsgs(
      SendServerPushRowMsg);
}

void ServerThreads::SendServerPushRowMsg(int32_t bg_id,
  ServerPushRowMsg *msg, bool last_msg) {
  //VLOG(0) << "msg = " << msg;
  //VLOG(0) << " msg->get_size() = " << msg->get_size()
  //      << " last_msg = " << last_msg;

  msg->get_version() = server_context_->server_obj_.GetBgVersion(bg_id);
  //VLOG(0) << "msg->get_version() set";
  if (last_msg) {
    msg->get_is_clock() = true;
    msg->get_clock() = server_context_->server_obj_.GetMinClock();
    MemTransfer::TransferMem(comm_bus_, bg_id, msg);
  } else {
    msg->get_is_clock() = false;
    size_t sent_size = (comm_bus_->*CommBusSendAny)(bg_id, msg->get_mem(),
                                                    msg->get_size());
    CHECK_EQ(sent_size, msg->get_size());
  }
}

void ServerThreads::SSPPushRowSubscribe(ServerRow *server_row,
                                        int32_t client_id) {
  //VLOG(0) << "ServerThreads client " << client_id << " subscibe to callback";
  server_row->Subscribe(client_id);
}

void *ServerThreads::ServerThreadMain(void *thread_id){
  int32_t my_id = *(reinterpret_cast<int32_t*>(thread_id));

  ThreadContext::RegisterThread(my_id);
  REGISTER_THREAD_FOR_STATS(false);

  // set up thread-specific server context
  SetUpServerContext();
  SetUpCommBus();

  pthread_barrier_wait(&init_barrier);

  InitServer();

  zmq::message_t zmq_msg;
  int32_t sender_id;
  MsgType msg_type;
  void *msg_mem;
  bool destroy_mem = false;
  while(1) {
    CommBusRecvAnyWrapper(&sender_id, &zmq_msg);

    msg_type = MsgBase::get_msg_type(zmq_msg.data());
    //VLOG(0) << "msg_type = " << msg_type;
    destroy_mem = false;

    if (msg_type == kMemTransfer) {
      MemTransferMsg mem_transfer_msg(zmq_msg.data());
      msg_mem = mem_transfer_msg.get_mem_ptr();
      msg_type = MsgBase::get_msg_type(msg_mem);
      destroy_mem = true;
    } else {
      msg_mem = zmq_msg.data();
    }

    switch (msg_type) {
    case kClientShutDown:
      {
	VLOG(0) << "get ClientShutDown from bg " << sender_id;
	bool shutdown = HandleShutDownMsg();
	if (shutdown) {
          VLOG(0) << "Server shutdown";
	  comm_bus_->ThreadDeregister();
	  FINALIZE_STATS();
	  return 0;
	}
	break;
      }
    case kCreateTable:
      {
	CreateTableMsg create_table_msg(msg_mem);
	HandleCreateTable(sender_id, create_table_msg);
	break;
      }
    case kRowRequest:
      {
	RowRequestMsg row_request_msg(msg_mem);
	HandleRowRequest(sender_id, row_request_msg);
      }
      break;
    case kClientSendOpLog:
      {
	VLOG(0) << "Received OpLog Msg!";
	ClientSendOpLogMsg client_send_oplog_msg(msg_mem);
	TIMER_BEGIN(0, SERVER_HANDLE_OPLOG_MSG);
	HandleOpLogMsg(sender_id, client_send_oplog_msg);
	TIMER_END(0, SERVER_HANDLE_OPLOG_MSG);
      }
      break;
    default:
      LOG(FATAL) << "Unrecognized message type " << msg_type;
    }

    if (destroy_mem)
      MemTransfer::DestroyTransferredMem(msg_mem);
  }
}

}
