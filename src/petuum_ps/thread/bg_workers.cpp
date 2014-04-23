// bg_workers.cpp
// author: jinliang

#include "petuum_ps/thread/ps_msgs.hpp"
#include "petuum_ps/thread/bg_workers.hpp"
#include "petuum_ps/thread/mem_transfer.hpp"
#include "petuum_ps/util/class_register.hpp"
#include "petuum_ps/oplog/serialized_oplog_reader.hpp"
#include "petuum_ps/client/ssp_client_row.hpp"
#include "petuum_ps/thread/ssp_push_row_request_oplog_mgr.hpp"
#include "petuum_ps/client/serialized_row_reader.hpp"
#include "petuum_ps/util/stats.hpp"
#include <utility>

namespace petuum {

std::vector<pthread_t> BgWorkers::threads_;
std::vector<int32_t> BgWorkers::thread_ids_;
std::map<int32_t, ClientTable* > * BgWorkers::tables_;
int32_t BgWorkers::id_st_;
pthread_barrier_t BgWorkers::init_barrier_;
pthread_barrier_t BgWorkers::create_table_barrier_;
boost::thread_specific_ptr<BgWorkers::BgContext> BgWorkers::bg_context_;
CommBus *BgWorkers::comm_bus_;

CommBus::RecvFunc BgWorkers::CommBusRecvAny;
CommBus::RecvTimeOutFunc BgWorkers::CommBusRecvTimeOutAny;
CommBus::SendFunc BgWorkers::CommBusSendAny;
BgWorkers::CreateClientRowFunc BgWorkers::MyCreateClientRow;
CommBus::RecvAsyncFunc BgWorkers::CommBusRecvAsyncAny;
CommBus::RecvWrapperFunc BgWorkers::CommBusRecvAnyWrapper;

std::mutex BgWorkers::system_clock_mtx_;
std::condition_variable BgWorkers::system_clock_cv_;
std::atomic_int_fast32_t BgWorkers::system_clock_;
VectorClockMT BgWorkers::bg_server_clock_;
BgWorkers::GetRowOpLogFunc BgWorkers::GetRowOpLog;
boost::unordered_map<int32_t, boost::unordered_map<int32_t, bool> >
BgWorkers::table_oplog_index_;

void BgWorkers::Init(std::map<int32_t, ClientTable* > *tables) {
  threads_.resize(GlobalContext::get_num_bg_threads());
  thread_ids_.resize(GlobalContext::get_num_bg_threads());
  tables_ = tables;
  id_st_ = GlobalContext::get_head_bg_id(GlobalContext::get_client_id());
  comm_bus_ = GlobalContext::comm_bus;

  int32_t my_client_id = GlobalContext::get_client_id();
  int32_t my_head_bg_id = GlobalContext::get_head_bg_id(my_client_id);
  for (int32_t i = 0; i < GlobalContext::get_num_bg_threads(); ++i) {
    bg_server_clock_.AddClock(my_head_bg_id + i, 0);
  }

  pthread_barrier_init(&init_barrier_, NULL,
    GlobalContext::get_num_bg_threads() + 1);
  pthread_barrier_init(&create_table_barrier_, NULL, 2);

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

  BgThreadMainFunc BgThreadMain;
  ConsistencyModel consistency_model = GlobalContext::get_consistency_model();
  switch(consistency_model) {
    case SSP:
      {
        BgThreadMain = SSPBgThreadMain;
        MyCreateClientRow = CreateSSPClientRow;
        GetRowOpLog = SSPGetRowOpLog;
      }
      break;
    case SSPPush:
      {
        BgThreadMain = SSPBgThreadMain;
        MyCreateClientRow = CreateClientRow;
        system_clock_ = 0;
        GetRowOpLog = SSPGetRowOpLog;
      }
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
  for(i = 0; i < GlobalContext::get_num_bg_threads(); ++i){
    thread_ids_[i] = id_st_ + i;
    int ret = pthread_create(&threads_[i], NULL, BgThreadMain,
      &thread_ids_[i]);
    CHECK_EQ(ret, 0);
  }
  pthread_barrier_wait(&init_barrier_);

  ThreadRegister();
}

void BgWorkers::ShutDown(){
  for(int i = 0; i < GlobalContext::get_num_bg_threads(); ++i){
    int ret = pthread_join(threads_[i], NULL);
    CHECK_EQ(ret, 0);
  }
}

void BgWorkers::ThreadRegister(){
  int i;
  for (i = 0; i < GlobalContext::get_num_bg_threads(); ++i) {
    int32_t bg_id = thread_ids_[i];
    ConnectToBg(bg_id);
  }
}

void BgWorkers::ThreadDeregister(){
  AppThreadDeregMsg msg;
  SendToAllLocalBgThreads(msg.get_mem(), msg.get_size());
}

bool BgWorkers::CreateTable(int32_t table_id,
  const ClientTableConfig &table_config){
  // send request
  {
    const TableInfo &table_info = table_config.table_info;
    BgCreateTableMsg bg_create_table_msg;
    bg_create_table_msg.get_table_id() = table_id;
    bg_create_table_msg.get_staleness() = table_info.table_staleness;
    bg_create_table_msg.get_row_type() = table_info.row_type;
    bg_create_table_msg.get_row_capacity() = table_info.row_capacity;
    bg_create_table_msg.get_process_cache_capacity()
      = table_config.process_cache_capacity;
    bg_create_table_msg.get_thread_cache_capacity()
      = table_config.thread_cache_capacity;
    bg_create_table_msg.get_oplog_capacity() = table_config.oplog_capacity;
    void *msg = bg_create_table_msg.get_mem();
    int32_t msg_size = bg_create_table_msg.get_size();

    size_t sent_size = comm_bus_->SendInProc(id_st_, msg, msg_size);
    CHECK_EQ((int32_t) sent_size, msg_size);
  }
  // waiting for response
  {
    zmq::message_t zmq_msg;
    int32_t sender_id;
    comm_bus_->RecvInProc(&sender_id, &zmq_msg);
    MsgType msg_type = MsgBase::get_msg_type(zmq_msg.data());
    CHECK_EQ(msg_type, kCreateTableReply);
  }
  return true;
}

void BgWorkers::WaitCreateTable(){
  pthread_barrier_wait(&create_table_barrier_);
}

bool BgWorkers::RequestRow(int32_t table_id, int32_t row_id, int32_t clock){
  {
    RowRequestMsg request_row_msg;
    request_row_msg.get_table_id() = table_id;
    request_row_msg.get_row_id() = row_id;
    request_row_msg.get_clock() = clock;

    //VLOG(0) << "BgWorkers request row clock = " << clock
    //	    << " table_id = " << table_id
    //	    << " row_id = " << row_id
    //	    << " thread id = " << ThreadContext::get_id();
    int32_t bg_id = GlobalContext::GetBgPartitionNum(row_id) + id_st_;
    size_t sent_size = comm_bus_->SendInProc(bg_id, request_row_msg.get_mem(),
				      request_row_msg.get_size());
    CHECK_EQ(sent_size, request_row_msg.get_size());
  }

  {
    zmq::message_t zmq_msg;
    int32_t sender_id;
    comm_bus_->RecvInProc(&sender_id, &zmq_msg);
    /*
    bool received = comm_bus_->RecvInProcAsync(&sender_id, &zmq_msg);
    while (!received) {
      found = comm_bus_->RecvInProcAsync(&sender_id, &zmq_msg);
      } */
    MsgType msg_type = MsgBase::get_msg_type(zmq_msg.data());
    CHECK_EQ(msg_type, kRowRequestReply);
  }
  return true;
}

void BgWorkers::RequestRowAsync(int32_t table_id, int32_t row_id,
                                int32_t clock){
  RowRequestMsg request_row_msg;
  request_row_msg.get_table_id() = table_id;
  request_row_msg.get_row_id() = row_id;
  request_row_msg.get_clock() = clock;

  int32_t bg_id = GlobalContext::GetBgPartitionNum(row_id) + id_st_;
  size_t sent_size = comm_bus_->SendInProc(bg_id, request_row_msg.get_mem(),
                                           request_row_msg.get_size());
  CHECK_EQ(sent_size, request_row_msg.get_size());
}

void BgWorkers::GetAsyncRowRequestReply() {
  zmq::message_t zmq_msg;
  int32_t sender_id;
  comm_bus_->RecvInProc(&sender_id, &zmq_msg);
  MsgType msg_type = MsgBase::get_msg_type(zmq_msg.data());
  CHECK_EQ(msg_type, kRowRequestReply);
}


void BgWorkers::ClockAllTables() {
  BgClockMsg bg_clock_msg;
  SendToAllLocalBgThreads(bg_clock_msg.get_mem(), bg_clock_msg.get_size());
}

void BgWorkers::SendOpLogsAllTables() {
  BgSendOpLogMsg bg_send_oplog_msg;
  SendToAllLocalBgThreads(bg_send_oplog_msg.get_mem(),
    bg_send_oplog_msg.get_size());
}

int32_t BgWorkers::GetSystemClock() {
  return static_cast<int32_t>(system_clock_.load());
}
void BgWorkers::WaitSystemClock(int32_t my_clock) {
  std::unique_lock<std::mutex> lock(system_clock_mtx_);
  // The bg threads might have advanced the clock after my last check.
  while (static_cast<int32_t>(system_clock_.load()) < my_clock) {
    VLOG(0) << "Wait " << my_clock;
    system_clock_cv_.wait(lock);
    VLOG(0) << "wake up";
  }
}

/* Private Functions */

void BgWorkers::ConnectToNameNodeOrServer(int32_t server_id){
  VLOG(0) << "ConnectToNameNodeOrServer server_id = " << server_id;
  ClientConnectMsg client_connect_msg;
  client_connect_msg.get_client_id() = GlobalContext::get_client_id();
  void *msg = client_connect_msg.get_mem();
  int32_t msg_size = client_connect_msg.get_size();

  if (comm_bus_->IsLocalEntity(server_id)) {
    VLOG(0) << "Connect to local server " << server_id;
    comm_bus_->ConnectTo(server_id, msg, msg_size);
  } else {
    VLOG(0) << "Connect to remote server " << server_id;
    HostInfo server_info = GlobalContext::get_host_info(server_id);
    std::string server_addr = server_info.ip + ":" + server_info.port;
    VLOG(0) << "server_addr = " << server_addr;
    comm_bus_->ConnectTo(server_id, server_addr, msg, msg_size);
  }
}

void BgWorkers::ConnectToBg(int32_t bg_id){
  AppConnectMsg app_connect_msg;
  void *msg = app_connect_msg.get_mem();
  int32_t msg_size = app_connect_msg.get_size();
  comm_bus_->ConnectTo(bg_id, msg, msg_size);
}

void BgWorkers::SendToAllLocalBgThreads(void *msg, int32_t size){
  int i;
  for(i = 0; i < GlobalContext::get_num_bg_threads(); ++i){
    int32_t sent_size = comm_bus_->SendInProc(thread_ids_[i], msg, size);
    CHECK_EQ(sent_size, size);
  }
}

ClientRow *BgWorkers::CreateSSPClientRow(int32_t clock, AbstractRow *row_data) {
  return reinterpret_cast<ClientRow*>(new SSPClientRow(clock, row_data));
}

ClientRow *BgWorkers::CreateClientRow(int32_t clock, AbstractRow *row_data) {
  return (new ClientRow(clock, row_data));
}

void BgWorkers::BgServerHandshake(){
  {
    // connect to name node
    int32_t name_node_id = GlobalContext::get_name_node_id();
    ConnectToNameNodeOrServer(name_node_id);

    // wait for ConnectServerMsg
    zmq::message_t zmq_msg;
    int32_t sender_id;
    if (comm_bus_->IsLocalEntity(name_node_id)) {
      comm_bus_->RecvInProc(&sender_id, &zmq_msg);
    }else{
      comm_bus_->RecvInterProc(&sender_id, &zmq_msg);
    }
    MsgType msg_type = MsgBase::get_msg_type(zmq_msg.data());
    CHECK_EQ(sender_id, name_node_id);
    CHECK_EQ(msg_type, kConnectServer) << "sender_id = " << sender_id;
  }

  // connect to servers
  {
    int32_t num_servers = GlobalContext::get_num_servers();
    std::vector<int32_t> server_ids = GlobalContext::get_server_ids();
    CHECK_EQ((size_t) num_servers, server_ids.size());
    for(int i = 0; i < num_servers; ++i){
      int32_t server_id = server_ids[i];
      VLOG(0) << "Connect to server " << server_id;
      ConnectToNameNodeOrServer(server_id);
    }
  }

  // get messages from servers for permission to start
  {
    int32_t num_started_servers = 0;
    for(num_started_servers = 0;
	// receive from all servers and name node
        num_started_servers < GlobalContext::get_num_servers() + 1;
	++num_started_servers){
      zmq::message_t zmq_msg;
      int32_t sender_id;
      (comm_bus_->*CommBusRecvAny)(&sender_id, &zmq_msg);
      MsgType msg_type = MsgBase::get_msg_type(zmq_msg.data());
      // TODO: in pushing mode, it may receive other types of message
      // from server
      CHECK_EQ(msg_type, kClientStart);
      VLOG(0) << "get kClientStart from " << sender_id;
    }
  }
}

// the app thread shall not submit another create table request before the
// current one returns as it is blocked waiting
void BgWorkers::HandleCreateTables(){
  for(int32_t num_created_tables = 0;
    num_created_tables < GlobalContext::get_num_tables();
    ++num_created_tables){
    int32_t table_id;
    int32_t sender_id;
    ClientTableConfig client_table_config;

    {
      zmq::message_t zmq_msg;
      comm_bus_->RecvInProc(&sender_id, &zmq_msg);
      MsgType msg_type = MsgBase::get_msg_type(zmq_msg.data());
      CHECK_EQ(msg_type, kBgCreateTable);
      BgCreateTableMsg bg_create_table_msg(zmq_msg.data());
      // set up client table config
      client_table_config.table_info.table_staleness
        = bg_create_table_msg.get_staleness();
      client_table_config.table_info.row_type
	= bg_create_table_msg.get_row_type();
      client_table_config.table_info.row_capacity
	= bg_create_table_msg.get_row_capacity();
      client_table_config.process_cache_capacity
        = bg_create_table_msg.get_process_cache_capacity();
      client_table_config.thread_cache_capacity
	= bg_create_table_msg.get_thread_cache_capacity();
      client_table_config.oplog_capacity
	= bg_create_table_msg.get_oplog_capacity();

      CreateTableMsg create_table_msg;
      create_table_msg.get_table_id() = bg_create_table_msg.get_table_id();
      create_table_msg.get_staleness() = bg_create_table_msg.get_staleness();
      create_table_msg.get_row_type() = bg_create_table_msg.get_row_type();
      create_table_msg.get_row_capacity()
	= bg_create_table_msg.get_row_capacity();
      table_id = create_table_msg.get_table_id();

      // send msg to name node
      int32_t name_node_id = GlobalContext::get_name_node_id();
      size_t sent_size = (comm_bus_->*CommBusSendAny)(name_node_id,
	create_table_msg.get_mem(), create_table_msg.get_size());
      CHECK_EQ(sent_size, create_table_msg.get_size());
    }

    // wait for response from name node
    {
      zmq::message_t zmq_msg;
      int32_t name_node_id;
      (comm_bus_->*CommBusRecvAny)(&name_node_id, &zmq_msg);
      MsgType msg_type = MsgBase::get_msg_type(zmq_msg.data());

      //VLOG(0) << "Received msgtype = " << msg_type;

      CHECK_EQ(msg_type, kCreateTableReply);
      CreateTableReplyMsg create_table_reply_msg(zmq_msg.data());
      CHECK_EQ(create_table_reply_msg.get_table_id(), table_id);

      ClientTable *client_table;
      try {
	client_table  = new ClientTable(table_id, client_table_config);
      } catch (std::bad_alloc &e) {
	LOG(FATAL) << "Bad alloc exception";
      }
      // not thread-safe
      (*tables_)[table_id] = client_table;

      //VLOG(0) << "Reply app thread " << sender_id;
      size_t sent_size = comm_bus_->SendInProc(sender_id, zmq_msg.data(),
        zmq_msg.size());
      CHECK_EQ(sent_size, zmq_msg.size());
    }
  }
}

void BgWorkers::CheckForwardRowRequestToServer(int32_t app_thread_id,
  RowRequestMsg &row_request_msg){

  int32_t table_id = row_request_msg.get_table_id();
  int32_t row_id = row_request_msg.get_row_id();
  int32_t clock = row_request_msg.get_clock();

  // Check if the row exists in process cache
  auto table_iter = tables_->find(table_id);
  CHECK(table_iter != tables_->end());
  {
    // check if it is in process storage
    ClientTable *table = table_iter->second;
    ProcessStorage &table_storage = table->get_process_storage();
    RowAccessor row_accessor;
    bool found = table_storage.Find(row_id, &row_accessor);
    if (found) {
      // TODO: do not send if it's PUSH mode
      if (row_accessor.GetClientRow()->GetClock() >= clock) {
	RowRequestReplyMsg row_request_reply_msg;
	size_t sent_size = comm_bus_->SendInProc(app_thread_id,
          row_request_reply_msg.get_mem(), row_request_reply_msg.get_size());
	CHECK_EQ(sent_size, row_request_reply_msg.get_size());
	return;
      }
    }
  }

  std::pair<int32_t, int32_t> request_key(table_id, row_id);
  RowRequestInfo row_request;
  row_request.app_thread_id = app_thread_id;
  row_request.clock = row_request_msg.get_clock();

  // Version in request denotes the update version that the row on server can
  // see. Which should be 1 less than the current version number.
  row_request.version = bg_context_->version - 1;

  //VLOG(0) << "check if request should be forwarded to server, version = "
  //     << row_request.version;

  bool should_be_sent
    = bg_context_->row_request_oplog_mgr->AddRowRequest(row_request, table_id,
                                                        row_id);

  //VLOG(0) << "should_be_sent = " << should_be_sent;

  if (should_be_sent) {
    int32_t server_id = GlobalContext::GetRowPartitionServerID(table_id,
      row_id);
    size_t sent_size = (comm_bus_->*CommBusSendAny)(server_id,
      row_request_msg.get_mem(), row_request_msg.get_size());
    CHECK_EQ(sent_size, row_request_msg.get_size());
  }
}

void BgWorkers::ApplyOpLogsToRowData(int32_t table_id,
                                     ClientTable *client_table, int32_t row_id,
                                     uint32_t version, AbstractRow *row_data) {

  if (version + 1 < bg_context_->version) {
    BgOpLog *bg_oplog
        = bg_context_->row_request_oplog_mgr->OpLogIterInit(version + 1,
          bg_context_->version - 1);
    uint32_t oplog_version;

    while (bg_oplog != NULL) {
      BgOpLogPartition *bg_oplog_partition = bg_oplog->Get(table_id);
      // OpLogs that are after (exclusively) version should be applied
      RowOpLog *row_oplog = bg_oplog_partition->FindOpLog(row_id);
      if (row_oplog != 0) {
        int32_t column_id;
        void *update;
        update = row_oplog->BeginIterate(&column_id);
        while (update != 0) {
          VLOG(0) << "ApplyOpLogs update = " << update;
          row_data->ApplyIncUnsafe(column_id, update);
          update = row_oplog->Next(&column_id);
        }
      }
      bg_oplog
          = bg_context_->row_request_oplog_mgr->OpLogIterNext(&oplog_version);
    }
  }

  TableOpLog &table_oplog = client_table->get_oplog();
  OpLogAccessor oplog_accessor;
  if (table_oplog.FindOpLog(row_id, &oplog_accessor)) {
    void *update;
    int32_t column_id;
    update = oplog_accessor.BeginIterate(&column_id);
    while (update != 0) {
      VLOG(0) << "ApplyOpLogs update = " << update;
      row_data->ApplyIncUnsafe(column_id, update);
      update = oplog_accessor.Next(&column_id);
    }
  }
}

void BgWorkers::HandleServerRowRequestReply(
    int32_t server_id,
    ServerRowRequestReplyMsg &server_row_request_reply_msg) {

  int32_t table_id = server_row_request_reply_msg.get_table_id();
  int32_t row_id = server_row_request_reply_msg.get_row_id();
  int32_t clock = server_row_request_reply_msg.get_clock();
  //VLOG(0) << "table_id = " << table_id;
  //VLOG(0) << "Server reply clock = " << clock;
  uint32_t version = server_row_request_reply_msg.get_version();

  auto table_iter = tables_->find(table_id);
  CHECK(table_iter != tables_->end()) << "Cannot find table " << table_id;
  ClientTable *client_table = table_iter->second;
  int32_t row_type = client_table->get_row_type();

  AbstractRow *row_data
    = ClassRegistry<AbstractRow>::GetRegistry().CreateObject(row_type);

  row_data->Deserialize(server_row_request_reply_msg.get_row_data(),
    server_row_request_reply_msg.get_row_size());

  bg_context_->row_request_oplog_mgr->ServerAcknowledgeVersion(server_id,
                                                               version);

  ApplyOpLogsToRowData(table_id, client_table, row_id, version, row_data);

  ClientRow *client_row = MyCreateClientRow(clock, row_data);
  client_table->get_process_storage().Insert(row_id, client_row);

  std::vector<int32_t> app_thread_ids;
  int32_t clock_to_request
    = bg_context_->row_request_oplog_mgr->InformReply(table_id, row_id, clock,
    bg_context_->version, &app_thread_ids);

  if (clock_to_request >= 0) {
    RowRequestMsg row_request_msg;
    row_request_msg.get_table_id() = table_id;
    row_request_msg.get_row_id() = row_id;
    row_request_msg.get_clock() = clock_to_request;
    int32_t server_id = GlobalContext::GetRowPartitionServerID(table_id,
      row_id);
    VLOG(0) << "send to server " << server_id;
    size_t sent_size = (comm_bus_->*CommBusSendAny)(server_id,
      row_request_msg.get_mem(), row_request_msg.get_size());
    CHECK_EQ(sent_size, row_request_msg.get_size());
  }

  std::pair<int32_t, int32_t> request_key(table_id, row_id);
  RowRequestReplyMsg row_request_reply_msg;

  for (int i = 0; i < (int) app_thread_ids.size(); ++i) {
    //LOG(0) << "Reply to app thread " << app_thread_ids[i];
    size_t sent_size = comm_bus_->SendInProc(app_thread_ids[i],
      row_request_reply_msg.get_mem(), row_request_reply_msg.get_size());
    CHECK_EQ(sent_size, row_request_reply_msg.get_size());
  }
}

void BgWorkers::ShutDownClean() {
  delete bg_context_->row_request_oplog_mgr;
  FINALIZE_STATS();
}

void BgWorkers::ApplyServerPushedRow(uint32_t version, void *mem,
  size_t mem_size) {

  SerializedRowReader row_reader(mem, mem_size);
  bool not_empty = row_reader.Restart();
  // no row to read
  if (!not_empty)
    return;

  int32_t table_id = 0;
  int32_t row_id = 0;
  size_t row_size = 0;
  const void *data = row_reader.Next(&table_id, &row_id, &row_size);

  int32_t curr_table_id = -1;
  int32_t row_type = 0;
  ClientTable *client_table = NULL;
  while (data != NULL) {
    VLOG(0) << "Get data = " << data << " table id = " << table_id
         << " row id = " << row_id << " row size = " << row_size;
    if (curr_table_id != table_id) {
      auto table_iter = tables_->find(table_id);
      CHECK(table_iter != tables_->end()) << "Cannot find table " << table_id;
      client_table = table_iter->second;
      row_type = client_table->get_row_type();
      curr_table_id = table_id;
    }
    AbstractRow *row_data
        = ClassRegistry<AbstractRow>::GetRegistry().CreateObject(row_type);
    row_data->Deserialize(data, row_size);
    VLOG(0) << "row data deserialized";
    ApplyOpLogsToRowData(table_id, client_table, row_id, version, row_data);
    ClientRow *client_row = MyCreateClientRow(0, row_data);
    client_table->get_process_storage().Insert(row_id, client_row);

    data = row_reader.Next(&table_id, &row_id, &row_size);
  }
}

void BgWorkers::CommBusRecvAnyBusy(int32_t *sender_id,
                                   zmq::message_t *zmq_msg) {
  bool received = (comm_bus_->*CommBusRecvAsyncAny)(sender_id, zmq_msg);
  while (!received) {
    received = (comm_bus_->*CommBusRecvAsyncAny)(sender_id, zmq_msg);
  }
}

void BgWorkers::CommBusRecvAnySleep(int32_t *sender_id,
                                    zmq::message_t *zmq_msg) {
  (comm_bus_->*CommBusRecvAny)(sender_id, zmq_msg);
}

bool BgWorkers::SSPGetRowOpLog(TableOpLog &table_oplog, int32_t row_id,
                               RowOpLog **row_oplog_ptr) {
  return table_oplog.GetEraseOpLog(row_id, row_oplog_ptr);
}

BgOpLog *BgWorkers::GetOpLogAndIndex() {
  std::vector<int32_t> server_ids = GlobalContext::get_server_ids();
  int32_t local_bg_index = ThreadContext::get_id() - id_st_;
  // get thread-specific data structure to assist oplog message creation
  // those maps may contain legacy data from previous runs
  std::map<int32_t, std::map<int32_t, size_t> > &server_table_oplog_size_map
    = bg_context_->server_table_oplog_size_map;
  std::map<int32_t, size_t> &table_num_bytes_by_server
      = bg_context_->table_server_oplog_size_map;

  BgOpLog *bg_oplog = new BgOpLog;

  for (auto table_iter = tables_->cbegin(); table_iter != tables_->cend();
       table_iter++) {
    int32_t table_id = table_iter->first;
    TableOpLog &table_oplog = table_iter->second->get_oplog();

    // Get OpLog index
    cuckoohash_map<int32_t, bool> *new_table_oplog_index_ptr
        = table_iter->second->GetAndResetOpLogIndex(local_bg_index);

    size_t table_update_size
        = table_iter->second->get_sample_row()->get_update_size();
    BgOpLogPartition *bg_table_oplog = new BgOpLogPartition(table_id,
                                                            table_update_size);

    // Memory layout of serialized OpLogs for one row:
    // 1. int32_t : num of rows
    // For each row
    // 1. a int32_t for row id
    // 2. a int32_t for number of updates
    // 3. an array of int32_t: column ids for updates
    // 4. an array of updates
    for(int i = 0; i < GlobalContext::get_num_servers(); ++i){
      int32_t server_id = server_ids[i];
      // 1. int32_t: number of rows
      table_num_bytes_by_server[server_id] = sizeof(int32_t);
    }

    for (auto oplog_index_iter = new_table_oplog_index_ptr->cbegin();
         !oplog_index_iter.is_end(); oplog_index_iter++) {
      int32_t row_id = oplog_index_iter->first;
      RowOpLog *row_oplog = 0;
      bool found = GetRowOpLog(table_oplog, row_id, &row_oplog);
      if (!found)
        continue;

      if (found && (row_oplog == 0)) {
        table_oplog_index_[table_id][row_id] = true;
        VLOG(0) << "found && row_oplog == 0";
        continue;
      }

      // update oplog message size
      int32_t server_id = GlobalContext::GetRowPartitionServerID(table_id,
                                                                 row_id);
      int32_t num_updates = row_oplog->GetSize();
      // 1) row id
      // 2) number of updates in that row
      // 3) total size for column ids
      // 4) total size for update array
      table_num_bytes_by_server[server_id] += sizeof(int32_t) + sizeof(int32_t)
        + sizeof(int32_t)*num_updates + table_update_size*num_updates;
      VLOG(0) << "Calling InsertOpLog";
      bg_table_oplog->InsertOpLog(row_id, row_oplog);
    }
    bg_oplog->Add(table_id, bg_table_oplog);
    delete new_table_oplog_index_ptr;

    for (auto server_iter = table_num_bytes_by_server.begin();
      server_iter != table_num_bytes_by_server.end(); server_iter++) {
      server_table_oplog_size_map[server_iter->first][table_id]
        = server_iter->second;
    }
  }
  return bg_oplog;
}

void BgWorkers::CreateOpLogMsgs(const BgOpLog *bg_oplog) {
  std::map<int32_t, std::map<int32_t, size_t> > &server_table_oplog_size_map
      = bg_context_->server_table_oplog_size_map;
  std::map<int32_t, ClientSendOpLogMsg* > &server_oplog_msg_map
    = bg_context_->server_oplog_msg_map;
  std::map<int32_t, size_t> &server_oplog_msg_size_map
    = bg_context_->server_oplog_msg_size_map;

  std::map<int32_t, std::map<int32_t, void*> > table_server_mem_map;

  for (auto server_iter = server_table_oplog_size_map.begin();
    server_iter != server_table_oplog_size_map.end(); server_iter++) {
    OpLogSerializer oplog_serializer;
    int32_t server_id = server_iter->first;
    server_oplog_msg_size_map[server_id]
        = oplog_serializer.Init(server_iter->second);

    server_oplog_msg_map[server_id]
      = new ClientSendOpLogMsg(server_oplog_msg_size_map[server_id]);
    oplog_serializer.AssignMem(server_oplog_msg_map[server_id]->get_data());

    for (auto table_iter = tables_->cbegin(); table_iter != tables_->cend();
      table_iter++) {
      int32_t table_id = table_iter->first;
      uint8_t *table_ptr
	= reinterpret_cast<uint8_t*>(oplog_serializer.GetTablePtr(table_id));
      // table id
      *(reinterpret_cast<int32_t*>(table_ptr)) = table_iter->first;
      // table update size
      *(reinterpret_cast<size_t*>(table_ptr + sizeof(int32_t)))
	= table_iter->second->get_sample_row()->get_update_size();
      // offset for table rows
      table_server_mem_map[table_id][server_id]
        = table_ptr + sizeof(int32_t) + sizeof(size_t);
    }
  }
  VLOG(0) << "Here";
  for (auto table_iter = tables_->cbegin(); table_iter != tables_->cend();
    table_iter++) {
    int32_t table_id = table_iter->first;
    BgOpLogPartition *oplog_partition = bg_oplog->Get(table_id);
    oplog_partition->SerializeByServer(&(table_server_mem_map[table_id]));
  }
}

void BgWorkers::HandleClockMsg(bool clock_advanced) {
  BgOpLog *bg_oplog = GetOpLogAndIndex();
  VLOG(0) << "Got OpLog and index";

  CreateOpLogMsgs(bg_oplog);
  VLOG(0) << "Created OpLogMsgs";
  std::map<int32_t, ClientSendOpLogMsg* > &server_oplog_msg_map
    = bg_context_->server_oplog_msg_map;
  for (auto oplog_msg_iter = server_oplog_msg_map.begin();
       oplog_msg_iter != server_oplog_msg_map.end(); oplog_msg_iter++) {
    oplog_msg_iter->second->get_is_clock() = clock_advanced;
    oplog_msg_iter->second->get_client_id() = GlobalContext::get_client_id();
    oplog_msg_iter->second->get_version() = bg_context_->version;
    int32_t server_id = oplog_msg_iter->first;

    MemTransfer::TransferMem(comm_bus_, server_id, oplog_msg_iter->second);
    // delete message after send
    delete oplog_msg_iter->second;
    oplog_msg_iter->second = 0;
  }
  VLOG(0) << "OpLogMsgs are sent out";

  bool tracked = bg_context_->row_request_oplog_mgr->AddOpLog(
      bg_context_->version, bg_oplog);
  ++bg_context_->version;
  bg_context_->row_request_oplog_mgr->InformVersionInc();
  if (!tracked) {
    delete bg_oplog;
  }
}

// Bg thread initialization logic:
// I. Establish connections with all server threads (app threads cannot send
// message to bg threads until this is done);
// II. Wait on a "Start" message from each server thread;
// III. Receive connections from all app threads. Server message (currently none
// for pull model) may come in at the same time.

void *BgWorkers::SSPBgThreadMain(void *thread_id) {

  int32_t my_id = *(reinterpret_cast<int32_t*>(thread_id));

  LOG(INFO) << "Bg Worker starts here, my_id = " << my_id;

  ThreadContext::RegisterThread(my_id);
  REGISTER_THREAD_FOR_STATS(false);

  int32_t num_connected_app_threads = 0;
  int32_t num_deregistered_app_threads = 0;
  int32_t num_shutdown_acked_servers = 0;

  bg_context_.reset(new BgContext);
  bg_context_->version = 0;
  switch (GlobalContext::get_consistency_model()) {
    case SSP:
      bg_context_->row_request_oplog_mgr = new SSPRowRequestOpLogMgr;
      break;
    case SSPPush:
      bg_context_->row_request_oplog_mgr = new SSPPushRowRequestOpLogMgr;
      break;
    default:
      LOG(FATAL) << "Unrecognized consistency model: "
                 << GlobalContext::get_consistency_model();
  }
  // set up serveral maps at intialization and reuse them to avoid
  // costly rebuilding
  {
    std::vector<int32_t> &server_ids = GlobalContext::get_server_ids();
    for (auto server_iter = server_ids.cbegin();
      server_iter != server_ids.cend(); server_iter++) {

      bg_context_->server_table_oplog_size_map.insert(
        std::make_pair(*server_iter, std::map<int32_t, size_t>()));

      bg_context_->server_oplog_msg_map.insert({*server_iter, 0});

      bg_context_->server_oplog_msg_size_map.insert({*server_iter, 0});

      bg_context_->table_server_oplog_size_map.insert({*server_iter, 0});

      bg_context_->server_vector_clock.AddClock(*server_iter);
    }
  }
  {
    CommBus::Config comm_config;
    comm_config.entity_id_ = my_id;
    comm_config.ltype_ = CommBus::kInProc;
    comm_bus_->ThreadRegister(comm_config);
  }

  // server handshake
  BgServerHandshake();

  pthread_barrier_wait(&init_barrier_);

  // get connection from init thread
  {
    zmq::message_t zmq_msg;
    int32_t sender_id;
    comm_bus_->RecvInProc(&sender_id, &zmq_msg);
    MsgType msg_type = MsgBase::get_msg_type(zmq_msg.data());
    CHECK_EQ(msg_type, kAppConnect) << "send_id = " << sender_id;
    ++num_connected_app_threads;
    CHECK(num_connected_app_threads <= GlobalContext::get_num_app_threads());
    VLOG(0) << "get connected from init thread " << sender_id;
  }

  if(my_id == id_st_){
    VLOG(0) << "head bg handles CreateTable";
    HandleCreateTables();
    zmq::message_t zmq_msg;
    int32_t sender_id;
    (comm_bus_->*CommBusRecvAny)(&sender_id, &zmq_msg);
    MsgType msg_type = MsgBase::get_msg_type(zmq_msg.data());
    CHECK_EQ(msg_type, kCreatedAllTables);
    pthread_barrier_wait(&create_table_barrier_);
  }

  zmq::message_t zmq_msg;
  int32_t sender_id;
  MsgType msg_type;
  void *msg_mem;
  bool destroy_mem = false;
  while (1) {
    CommBusRecvAnyWrapper(&sender_id, &zmq_msg);

    msg_type = MsgBase::get_msg_type(zmq_msg.data());
    destroy_mem = false;

    if (msg_type == kMemTransfer) {
      //VLOG(0) << "Received kMemTransfer message from " << sender_id;
      MemTransferMsg mem_transfer_msg(zmq_msg.data());
      msg_mem = mem_transfer_msg.get_mem_ptr();
      msg_type = MsgBase::get_msg_type(msg_mem);
      destroy_mem = true;
    } else {
      msg_mem = zmq_msg.data();
    }

    //VLOG(0) << "msg_type = " << msg_type;
    switch (msg_type) {
      case kAppConnect:
        {
          ++num_connected_app_threads;
          CHECK(num_connected_app_threads
                <= GlobalContext::get_num_app_threads())
              << "num_connected_app_threads = " << num_connected_app_threads
              << " get_num_app_threads() = "
              << GlobalContext::get_num_app_threads();
        }
        break;
      case kAppThreadDereg:
        {
          ++num_deregistered_app_threads;
          if (num_deregistered_app_threads
              == GlobalContext::get_num_app_threads()) {
            ClientShutDownMsg msg;
            int32_t name_node_id = GlobalContext::get_name_node_id();
            (comm_bus_->*CommBusSendAny)(name_node_id, msg.get_mem(),
              msg.get_size());
            int32_t num_servers = GlobalContext::get_num_servers();
            std::vector<int32_t> &server_ids = GlobalContext::get_server_ids();
            for (int i = 0; i < num_servers; ++i) {
              int32_t server_id = server_ids[i];
              (comm_bus_->*CommBusSendAny)(server_id, msg.get_mem(),
	        msg.get_size());
            }
          }
        }
        break;
      case kServerShutDownAck:
        {
          ++num_shutdown_acked_servers;
          VLOG(0) << "get ServerShutDownAck from server " << sender_id;
          if (num_shutdown_acked_servers
              == GlobalContext::get_num_servers() + 1) {
	    VLOG(0) << "Bg worker " << my_id << " shutting down";
	    comm_bus_->ThreadDeregister();
	    ShutDownClean();
	    return 0;
	}
      }
      break;
    case kRowRequest:
      {
	//VLOG(0) << "Get RowRequest";
	RowRequestMsg row_request_msg(msg_mem);
	CheckForwardRowRequestToServer(sender_id, row_request_msg);
      }
      break;
    case kServerRowRequestReply:
      {
	ServerRowRequestReplyMsg server_row_request_reply_msg(msg_mem);
	HandleServerRowRequestReply(sender_id, server_row_request_reply_msg);
      }
      break;
    case kBgClock:
      {
        HandleClockMsg(true);
      }
      break;
      case kBgSendOpLog:
      {
        HandleClockMsg(false);
      }
      break;
      case kServerPushRow:
        {
          ServerPushRowMsg server_push_row_msg(msg_mem);
          VLOG(0) << "Received server push row msg";
          uint32_t version = server_push_row_msg.get_version();
          bg_context_->row_request_oplog_mgr->ServerAcknowledgeVersion(
              sender_id, version);
          // Need to apply the new rows before waking up the app threads
          ApplyServerPushedRow(version, server_push_row_msg.get_data(),
                               server_push_row_msg.get_avai_size());
          VLOG(0) << "Apply ServerPushedRow done";
          bool is_clock = server_push_row_msg.get_is_clock();
          if (is_clock) {
            int32_t server_clock = server_push_row_msg.get_clock();
            VLOG(0) << "is clock, server_clock = " << server_clock;
            CHECK_EQ(bg_context_->server_vector_clock.get_clock(sender_id) + 1,
                     server_clock);
            int32_t new_clock = bg_context_->server_vector_clock.Tick(
               sender_id);
            VLOG(0) << "thread id = " << my_id
                    << " received clock from " << sender_id
                    << " new clock = " << new_clock;
            if (new_clock) {
              int32_t new_system_clock = bg_server_clock_.Tick(my_id);
              VLOG(0) << "new_system_clock = " << new_system_clock;
              if (new_system_clock) {
                system_clock_ += 1;
                std::unique_lock<std::mutex> lock(system_clock_mtx_);
                VLOG(0) << "system_clock_cv_.notify_all()";
                system_clock_cv_.notify_all();
              }
            }
          }
        }
        break;
      default:
        LOG(FATAL) << "Unrecognized type " << msg_type;
    }

    if (destroy_mem)
      MemTransfer::DestroyTransferredMem(msg_mem);
  }

  return 0;
}

}  // namespace petuum
