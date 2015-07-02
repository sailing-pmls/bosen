#include <petuum_ps/server/server_thread.hpp>
#include <petuum_ps_common/thread/msg_base.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps/thread/ps_msgs.hpp>
#include <petuum_ps_common/util/stats.hpp>
#include <petuum_ps_common/thread/mem_transfer.hpp>
#include <petuum_ps/thread/numa_mgr.hpp>
#include <unistd.h>
namespace petuum {

bool ServerThread::WaitMsgBusy(int32_t *sender_id, zmq::message_t *zmq_msg,
                               long timeout_milli __attribute__ ((unused)) ) {
  bool received = (GlobalContext::comm_bus->*(
      GlobalContext::comm_bus->RecvAsyncAny_))(sender_id, zmq_msg);
  while (!received) {
    received = (GlobalContext::comm_bus->*(
        GlobalContext::comm_bus->RecvAsyncAny_))(sender_id, zmq_msg);
  }
  return true;
}

bool ServerThread::WaitMsgSleep(int32_t *sender_id, zmq::message_t *zmq_msg,
                                long timeout_milli __attribute__ ((unused)) ) {
  (GlobalContext::comm_bus->*(
      GlobalContext::comm_bus->RecvAny_))(sender_id, zmq_msg);

  return true;
}

bool ServerThread::WaitMsgTimeOut(int32_t *sender_id, zmq::message_t *zmq_msg,
                                  long timeout_milli) {
  bool received = (GlobalContext::comm_bus->*(
      GlobalContext::comm_bus->RecvTimeOutAny_))(
          sender_id, zmq_msg, timeout_milli);
  return received;
}

void ServerThread::InitWhenStart() {
  SetWaitMsg();
}

void ServerThread::SetWaitMsg() {
  if (GlobalContext::get_aggressive_cpu()) {
    WaitMsg_ = WaitMsgBusy;
  } else {
    WaitMsg_ = WaitMsgSleep;
  }
}

void ServerThread::SetUpCommBus() {
  CommBus::Config comm_config;
  comm_config.entity_id_ = my_id_;

  if (GlobalContext::get_num_clients() > 1) {
    comm_config.ltype_ = CommBus::kInProc | CommBus::kInterProc;
    HostInfo host_info = GlobalContext::get_server_info(my_id_);
    comm_config.network_addr_ = host_info.ip + ":" + host_info.port;
  } else {
    comm_config.ltype_ = CommBus::kInProc;
  }

  comm_bus_->ThreadRegister(comm_config);
}

void ServerThread::ConnectToNameNode() {
  int32_t name_node_id = GlobalContext::get_name_node_id();

  ServerConnectMsg server_connect_msg;
  void *msg = server_connect_msg.get_mem();
  int32_t msg_size = server_connect_msg.get_size();

  if (comm_bus_->IsLocalEntity(name_node_id)) {
    comm_bus_->ConnectTo(name_node_id, msg, msg_size);
  } else {
    HostInfo name_node_info = GlobalContext::get_name_node_info();
    std::string name_node_addr = name_node_info.ip + ":" + name_node_info.port;
    comm_bus_->ConnectTo(name_node_id, name_node_addr, msg, msg_size);
  }
}

int32_t ServerThread::GetConnection(bool *is_client, int32_t *client_id) {
  int32_t sender_id;
  zmq::message_t zmq_msg;
  (comm_bus_->*(comm_bus_->RecvAny_))(&sender_id, &zmq_msg);
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

void ServerThread::SendToAllBgThreads(MsgBase *msg) {
  for (const auto &bg_worker_id : bg_worker_ids_) {
    size_t sent_size = (comm_bus_->*(comm_bus_->SendAny_))(
        bg_worker_id, msg->get_mem(), msg->get_size());
    CHECK_EQ(sent_size, msg->get_size());
  }
}

void ServerThread::InitServer() {
  ConnectToNameNode();

  int32_t num_bgs;
  for (num_bgs = 0; num_bgs < GlobalContext::get_num_clients();
    ++num_bgs) {
    int32_t client_id;
    bool is_client;
    int32_t bg_id = GetConnection(&is_client, &client_id);
    CHECK(is_client);
    bg_worker_ids_[num_bgs] = bg_id;
    msg_tracker_.AddEntity(bg_id);
  }

  server_obj_.Init(my_id_, bg_worker_ids_, &msg_tracker_);

  for (auto id : bg_worker_ids_) {
    client_progress_clock_.AddClock(id, 0);
  }

  ClientStartMsg client_start_msg;
  SendToAllBgThreads(reinterpret_cast<MsgBase*>(&client_start_msg));
}

bool ServerThread::HandleShutDownMsg() {
  // When num_shutdown_bgs reaches the total number of clients, the server
  // reply to each bg with a ShutDownReply message
  ++num_shutdown_bgs_;
  if (num_shutdown_bgs_ == GlobalContext::get_num_clients()) {
    if (pending_clock_push_row_
        || msg_tracker_.PendingAcks()) {
      pending_shut_down_ = true;
      return false;
    }
    SendServerShutDownAcks();
    return true;
  }
  return false;
}

void ServerThread::HandleCreateTable(int32_t sender_id,
                                     CreateTableMsg &create_table_msg) {
  int32_t table_id = create_table_msg.get_table_id();
  TableInfo table_info = create_table_msg.get_table_info();
  //LOG(INFO) << "server table logic = " << table_info.server_table_logic
  //        << " version maintain = " << table_info.version_maintain;

  server_obj_.CreateTable(table_id, table_info);

  min_table_staleness_
      = std::min(min_table_staleness_,
                 table_info.table_staleness);

  // I'm not name node
  CreateTableReplyMsg create_table_reply_msg;
  create_table_reply_msg.get_table_id() = table_id;
  size_t sent_size = (comm_bus_->*(comm_bus_->SendAny_))(sender_id,
    create_table_reply_msg.get_mem(), create_table_reply_msg.get_size());
  CHECK_EQ(sent_size, create_table_reply_msg.get_size());


  //LOG(INFO) << "table_info.table_staleness = "
  //        << table_info.table_staleness
  //        << " table_id = " << create_table_msg.get_table_id();
}

void ServerThread::HandleRowRequest(int32_t sender_id,
                                    RowRequestMsg &row_request_msg) {
  int32_t table_id = row_request_msg.get_table_id();
  int32_t row_id = row_request_msg.get_row_id();
  int32_t clock = row_request_msg.get_clock();
  int32_t server_clock = server_obj_.GetMinClock();
  if (server_clock < clock) {
    // not fresh enough, wait
    server_obj_.AddRowRequest(sender_id, table_id, row_id, clock);
    return;
  }

  uint32_t version = server_obj_.GetBgVersion(sender_id);
  ServerRow *server_row = server_obj_.FindCreateRow(table_id, row_id);
  RowSubscribe(server_row, GlobalContext::thread_id_to_client_id(sender_id));

  ReplyRowRequest(sender_id, server_row, table_id, row_id, server_clock,
                  version);
}

void ServerThread::ReplyRowRequest(int32_t bg_id, ServerRow *server_row,
                                   int32_t table_id, int32_t row_id,
                                   int32_t server_clock, uint32_t version) {
  size_t row_size = server_row->SerializedSize();

  ServerRowRequestReplyMsg server_row_request_reply_msg(row_size);
  server_row_request_reply_msg.get_table_id() = table_id;
  server_row_request_reply_msg.get_row_id() = row_id;
  server_row_request_reply_msg.get_clock() = server_clock;
  server_row_request_reply_msg.get_version() = version;

  row_size = server_row->Serialize(server_row_request_reply_msg.get_row_data());

  server_row_request_reply_msg.get_row_size() = row_size;

  MemTransfer::TransferMem(comm_bus_, bg_id, &server_row_request_reply_msg);
  server_obj_.RowSent(table_id, row_id, server_row, 1);
}

void ServerThread::HandleOpLogMsg(int32_t sender_id,
                                  ClientSendOpLogMsg &client_send_oplog_msg) {
  //LOG(INFO) << __func__;
  bool is_clock = client_send_oplog_msg.get_is_clock();

  uint32_t version = client_send_oplog_msg.get_version();
  int32_t bg_clock = client_send_oplog_msg.get_bg_clock();
  uint64_t seq = client_send_oplog_msg.get_seq_num();

  STATS_SERVER_ADD_PER_CLOCK_OPLOG_SIZE(client_send_oplog_msg.get_size());

  //LOG(INFO) << "server_recv_oplog, is_clock = " << is_clock
  //        << " from " << sender_id
  //        << " size = " << client_send_oplog_msg.get_size()
  //        << " " << my_id_;

  STATS_SERVER_ACCUM_APPLY_OPLOG_BEGIN();
  server_obj_.ApplyOpLogUpdateVersion(
      client_send_oplog_msg.get_data(), client_send_oplog_msg.get_avai_size(),
      sender_id, version);
  STATS_SERVER_ACCUM_APPLY_OPLOG_END();

  bool clock_changed = false;
  int32_t old_clock = server_obj_.GetMinClock();
  int32_t new_clock = old_clock;

  if (is_clock) {
    clock_changed = server_obj_.ClockUntil(sender_id, bg_clock);
    //LOG(INFO)  << "server_recv_oplog, is_clock = " << is_clock
    //         << " clock = " << bg_clock
    //         << " from " << sender_id
    //         << " size = " << client_send_oplog_msg.get_size()
    //         << " clock changed = " << clock_changed
    //         << " " << my_id_;
    if (clock_changed) {
      //LOG(INFO)  << "server_recv_oplog, is_clock = " << is_clock
      //         << " clock = " << bg_clock
      //         << " from " << sender_id
      //         << " size = " << client_send_oplog_msg.get_size()
      //         << " clock changed = " << clock_changed
      //         << " " << my_id_;
      std::vector<ServerRowRequest> requests;
      server_obj_.GetFulfilledRowRequests(&requests);
      for (auto request_iter = requests.begin();
	   request_iter != requests.end(); request_iter++) {
	int32_t table_id = request_iter->table_id;
	int32_t row_id = request_iter->row_id;
	int32_t bg_id = request_iter->bg_id;
	uint32_t version = server_obj_.GetBgVersion(bg_id);
	ServerRow *server_row = server_obj_.FindCreateRow(table_id, row_id);
        RowSubscribe(server_row,
                     GlobalContext::thread_id_to_client_id(bg_id));
	int32_t server_clock = server_obj_.GetMinClock();
	ReplyRowRequest(bg_id, server_row, table_id, row_id, server_clock,
                        version);
      }
    }
  } else if (my_id_ == 1 && GlobalContext::get_suppression_on()) {
    AdjustSuppressionLevel(sender_id, bg_clock);
  }

  if (clock_changed) {
    ClockNotice();
    ServerPushRow();
  }

  SendOpLogAckMsg(sender_id, server_obj_.GetBgVersion(sender_id),
                  seq);

  if (is_clock && clock_changed) {
    new_clock = server_obj_.GetMinClock();
    for (int i = 0; i < (new_clock - old_clock); ++i) {
      STATS_SERVER_CLOCK();
    }
  }
}

long ServerThread::ServerIdleWork() {
  return 0;
}

long ServerThread::ResetServerIdleMilli() {
  return 0;
}

void ServerThread::HandleEarlyCommOn() { }

void ServerThread::HandleEarlyCommOff() { }

void ServerThread::HandleBgServerPushRowAck(
    int32_t bg_id, uint64_t ack_seq) { }

void ServerThread::SendOpLogAckMsg(int32_t bg_id, uint32_t version,
                                   uint64_t seq) {
  ServerOpLogAckMsg server_oplog_ack_msg;
  server_oplog_ack_msg.get_ack_version() = version;
  server_oplog_ack_msg.get_ack_num() = seq;

  size_t sent_size = (GlobalContext::comm_bus->*(
      GlobalContext::comm_bus->SendAny_))(
          bg_id, server_oplog_ack_msg.get_mem(),
          server_oplog_ack_msg.get_size());
  CHECK_EQ(sent_size, server_oplog_ack_msg.get_size());
}

void ServerThread::SendServerShutDownAcks() {
  ServerShutDownAckMsg shut_down_ack_msg;
  size_t msg_size = shut_down_ack_msg.get_size();
  int i;
  for (i = 0; i < GlobalContext::get_num_clients(); ++i) {
    int32_t bg_id = bg_worker_ids_[i];
    size_t sent_size = (comm_bus_->*(comm_bus_->SendAny_))(
        bg_id, shut_down_ack_msg.get_mem(), msg_size);
    CHECK_EQ(msg_size, sent_size);
  }
}

void ServerThread::ShutDownServer() {
  comm_bus_->ThreadDeregister();
  STATS_DEREGISTER_THREAD();
}

void ServerThread::PrepareBeforeInfiniteLoop() { }

void ServerThread::ClockNotice() { }

void ServerThread::AdjustSuppressionLevel(int32_t bg_id, int32_t bg_clock) { }

void *ServerThread::operator() () {

  ThreadContext::RegisterThread(my_id_);

  //NumaMgr::ConfigureServerThread();

  STATS_REGISTER_THREAD(kServerThread);

  SetUpCommBus();

  pthread_barrier_wait(init_barrier_);

  InitServer();

  zmq::message_t zmq_msg;
  int32_t sender_id;
  MsgType msg_type = kNonExist;
  void *msg_mem;
  bool destroy_mem = false;
  long timeout_milli = -1;
  PrepareBeforeInfiniteLoop();
  while(1) {
    //LOG(INFO) << "timeout_milli = " << timeout_milli;
    bool received = WaitMsg_(&sender_id, &zmq_msg, timeout_milli);
    //LOG(INFO) << "received " << received;
    if (!received) {
      timeout_milli = ServerIdleWork();
      continue;
    } else {
      timeout_milli = ResetServerIdleMilli();
    }

    msg_type = MsgBase::get_msg_type(zmq_msg.data());
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
          bool shutdown = HandleShutDownMsg();
          if (shutdown) {
            CHECK(!msg_tracker_.PendingAcks());
            ShutDownServer();
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
          ClientSendOpLogMsg client_send_oplog_msg(msg_mem);

          HandleOpLogMsg(sender_id, client_send_oplog_msg);
          STATS_SERVER_OPLOG_MSG_RECV_INC_ONE();
        }
      break;
      case kEarlyCommOn:
        {
          HandleEarlyCommOn();
        }
        break;
      case kEarlyCommOff:
        {
          HandleEarlyCommOff();
        }
        break;
      case kBgServerPushRowAck:
        {
          BgServerPushRowAckMsg msg(msg_mem);
          HandleBgServerPushRowAck(sender_id, msg.get_ack_num());

          if (!pending_clock_push_row_
              && !msg_tracker_.PendingAcks()
              && pending_shut_down_) {
            pending_shut_down_ = false;
            CHECK(!msg_tracker_.PendingAcks());
            SendServerShutDownAcks();
            ShutDownServer();
            return 0;
          }
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
