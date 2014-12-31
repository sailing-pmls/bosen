#include <petuum_ps/thread/abstract_bg_worker.hpp>

#include <petuum_ps/thread/ps_msgs.hpp>
#include <petuum_ps/thread/numa_mgr.hpp>
#include <petuum_ps_common/util/class_register.hpp>
#include <petuum_ps/client/oplog_serializer.hpp>
#include <petuum_ps/client/ssp_client_row.hpp>
#include <petuum_ps_common/util/stats.hpp>
#include <petuum_ps_common/comm_bus/comm_bus.hpp>
#include <petuum_ps_common/thread/mem_transfer.hpp>
#include <petuum_ps/thread/context.hpp>
#include <glog/logging.h>
#include <utility>
#include <limits.h>
#include <algorithm>

namespace petuum {

AbstractBgWorker::AbstractBgWorker(int32_t id,
                   int32_t comm_channel_idx,
                   std::map<int32_t, ClientTable* > *tables,
                   pthread_barrier_t *init_barrier,
                   pthread_barrier_t *create_table_barrier):
    my_id_(id),
    my_comm_channel_idx_(comm_channel_idx),
    tables_(tables),
    version_(0),
    client_clock_(0),
    clock_has_pushed_(-1),
    comm_bus_(GlobalContext::comm_bus),
    init_barrier_(init_barrier),
    create_table_barrier_(create_table_barrier) {
  GlobalContext::GetServerThreadIDs(my_comm_channel_idx_, &(server_ids_));
  for (const auto &server_id : server_ids_) {
    server_table_oplog_size_map_.insert(
        std::make_pair(server_id, std::map<int32_t, size_t>()));
    server_oplog_msg_map_.insert({server_id, 0});
    table_num_bytes_by_server_.insert({server_id, 0});
  }
}

AbstractBgWorker::~AbstractBgWorker() {
  for (auto &serializer_pair : row_oplog_serializer_map_) {
    delete serializer_pair.second;
  }
}

void AbstractBgWorker::ShutDown() {
  Join();
}

void AbstractBgWorker::AppThreadRegister() {
  AppConnectMsg app_connect_msg;
  void *msg = app_connect_msg.get_mem();
  size_t msg_size = app_connect_msg.get_size();

  comm_bus_->ConnectTo(my_id_, msg, msg_size);
}

void AbstractBgWorker::AppThreadDeregister() {
  AppThreadDeregMsg msg;
  size_t sent_size = SendMsg(reinterpret_cast<MsgBase*>(&msg));
  CHECK_EQ(sent_size, msg.get_size());
}

bool AbstractBgWorker::CreateTable(int32_t table_id,
                           const ClientTableConfig& table_config) {
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

    bg_create_table_msg.get_oplog_dense_serialized()
        = table_info.oplog_dense_serialized;
    bg_create_table_msg.get_row_oplog_type()
        = table_info.row_oplog_type;
    bg_create_table_msg.get_dense_row_oplog_capacity()
        = table_info.dense_row_oplog_capacity;

    bg_create_table_msg.get_oplog_type()
        = table_config.oplog_type;
    bg_create_table_msg.get_append_only_oplog_type()
        = table_config.append_only_oplog_type;
    bg_create_table_msg.get_append_only_buff_capacity()
        = table_config.append_only_buff_capacity;
    bg_create_table_msg.get_per_thread_append_only_buff_pool_size()
        = table_config.per_thread_append_only_buff_pool_size;
    bg_create_table_msg.get_bg_apply_append_oplog_freq()
        = table_config.bg_apply_append_oplog_freq;
    bg_create_table_msg.get_process_storage_type()
        = table_config.process_storage_type;
    bg_create_table_msg.get_no_oplog_replay()
        = table_config.no_oplog_replay;

    size_t sent_size = SendMsg(
        reinterpret_cast<MsgBase*>(&bg_create_table_msg));
    CHECK_EQ((int32_t) sent_size, bg_create_table_msg.get_size());
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

bool AbstractBgWorker::RequestRow(int32_t table_id, int32_t row_id, int32_t clock) {
  {
    RowRequestMsg request_row_msg;
    request_row_msg.get_table_id() = table_id;
    request_row_msg.get_row_id() = row_id;
    request_row_msg.get_clock() = clock;
    request_row_msg.get_forced_request() = false;

    size_t sent_size = SendMsg(reinterpret_cast<MsgBase*>(&request_row_msg));
    CHECK_EQ(sent_size, request_row_msg.get_size());
  }

  {
    zmq::message_t zmq_msg;
    int32_t sender_id;
    comm_bus_->RecvInProc(&sender_id, &zmq_msg);

    MsgType msg_type = MsgBase::get_msg_type(zmq_msg.data());
    CHECK_EQ(msg_type, kRowRequestReply);
  }
  return true;
}

void AbstractBgWorker::RequestRowAsync(int32_t table_id, int32_t row_id,
                                       int32_t clock, bool forced) {
  RowRequestMsg request_row_msg;
  request_row_msg.get_table_id() = table_id;
  request_row_msg.get_row_id() = row_id;
  request_row_msg.get_clock() = clock;
  request_row_msg.get_forced_request() = forced;

  size_t sent_size = SendMsg(reinterpret_cast<MsgBase*>(&request_row_msg));
  CHECK_EQ(sent_size, request_row_msg.get_size());
}

void AbstractBgWorker::GetAsyncRowRequestReply() {
  zmq::message_t zmq_msg;
  int32_t sender_id;
  comm_bus_->RecvInProc(&sender_id, &zmq_msg);
  MsgType msg_type = MsgBase::get_msg_type(zmq_msg.data());
  CHECK_EQ(msg_type, kRowRequestReply);
}

void AbstractBgWorker::SignalHandleAppendOnlyBuffer(int32_t table_id) {
  BgHandleAppendOpLogMsg handle_append_oplog_msg;
  handle_append_oplog_msg.get_table_id() = table_id;

  size_t sent_size = SendMsg(reinterpret_cast<MsgBase*>(&handle_append_oplog_msg));
  CHECK_EQ(sent_size, handle_append_oplog_msg.get_size());
}

void AbstractBgWorker::ClockAllTables() {
  BgClockMsg bg_clock_msg;
  size_t sent_size = SendMsg(reinterpret_cast<MsgBase*>(&bg_clock_msg));
  CHECK_EQ(sent_size, bg_clock_msg.get_size());
}

void AbstractBgWorker::SendOpLogsAllTables() {
  BgSendOpLogMsg bg_send_oplog_msg;
  size_t sent_size = SendMsg(reinterpret_cast<MsgBase*>(&bg_send_oplog_msg));
  CHECK_EQ(sent_size, bg_send_oplog_msg.get_size());
}

void AbstractBgWorker::InitWhenStart() {
  SetWaitMsg();
  CreateRowRequestOpLogMgr();
}

bool AbstractBgWorker::WaitMsgBusy(int32_t *sender_id, zmq::message_t *zmq_msg,
                           long timeout_milli __attribute__ ((unused)) ) {
  bool received = (GlobalContext::comm_bus->*
                   (GlobalContext::comm_bus->RecvAsyncAny_))(sender_id,
                                               zmq_msg);
  while (!received)
    received = (GlobalContext::comm_bus->*
                (GlobalContext::comm_bus->RecvAsyncAny_))(sender_id,
                                                          zmq_msg);
  return true;
}

bool AbstractBgWorker::WaitMsgSleep(int32_t *sender_id, zmq::message_t *zmq_msg,
                            long timeout_milli __attribute__ ((unused)) ) {
  (GlobalContext::comm_bus->*
   (GlobalContext::comm_bus->RecvAny_))(sender_id, zmq_msg);
  return true;
}

bool AbstractBgWorker::WaitMsgTimeOut(int32_t *sender_id, zmq::message_t *zmq_msg,
                              long timeout_milli) {
  bool received = (GlobalContext::comm_bus->*
                   (GlobalContext::comm_bus->RecvTimeOutAny_))(
                       sender_id, zmq_msg, timeout_milli);
  return received;
}

size_t AbstractBgWorker::GetDenseSerializedRowOpLogSize(
    AbstractRowOpLog *row_oplog) {
  return row_oplog->GetDenseSerializedSize();
}

size_t AbstractBgWorker::GetSparseSerializedRowOpLogSize(
    AbstractRowOpLog *row_oplog) {
  row_oplog->ClearZerosAndGetNoneZeroSize();
  return row_oplog->GetSparseSerializedSize();
}

void AbstractBgWorker::SetWaitMsg() {
  if (GlobalContext::get_aggressive_cpu()) {
    WaitMsg_ = WaitMsgBusy;
  } else {
    WaitMsg_ = WaitMsgSleep;
  }
}

void AbstractBgWorker::InitCommBus() {
  CommBus::Config comm_config;
  comm_config.entity_id_ = my_id_;
  comm_config.ltype_ = CommBus::kInProc;
  comm_bus_->ThreadRegister(comm_config);
}

void AbstractBgWorker::BgServerHandshake() {
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
    for (const auto &server_id : server_ids_) {
      ConnectToNameNodeOrServer(server_id);
    }
  }

  // get messages from servers for permission to start
  {
    int32_t num_started_servers = 0;
    for (num_started_servers = 0;
         // receive from all servers and name node
         num_started_servers < GlobalContext::get_num_clients() + 1;
         ++num_started_servers) {
      zmq::message_t zmq_msg;
      int32_t sender_id;
      (comm_bus_->*(comm_bus_->RecvAny_))(&sender_id, &zmq_msg);
      MsgType msg_type = MsgBase::get_msg_type(zmq_msg.data());

      CHECK_EQ(msg_type, kClientStart);
    }
  }
}

void AbstractBgWorker::HandleCreateTables() {
  for (int32_t num_created_tables = 0;
       num_created_tables < GlobalContext::get_num_tables();
       ++num_created_tables) {
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

      client_table_config.table_info.oplog_dense_serialized
          = bg_create_table_msg.get_oplog_dense_serialized();
      client_table_config.table_info.row_oplog_type
          = bg_create_table_msg.get_row_oplog_type();
      client_table_config.table_info.dense_row_oplog_capacity
          = bg_create_table_msg.get_dense_row_oplog_capacity();

      client_table_config.oplog_type
          = bg_create_table_msg.get_oplog_type();
      client_table_config.append_only_oplog_type
          = bg_create_table_msg.get_append_only_oplog_type();
      client_table_config.append_only_buff_capacity
          = bg_create_table_msg.get_append_only_buff_capacity();
      client_table_config.per_thread_append_only_buff_pool_size
          = bg_create_table_msg.get_per_thread_append_only_buff_pool_size();
      client_table_config.bg_apply_append_oplog_freq
          = bg_create_table_msg.get_bg_apply_append_oplog_freq();
      client_table_config.process_storage_type
          = bg_create_table_msg.get_process_storage_type();
      client_table_config.no_oplog_replay
          = bg_create_table_msg.get_no_oplog_replay();

      CreateTableMsg create_table_msg;
      create_table_msg.get_table_id() = bg_create_table_msg.get_table_id();
      create_table_msg.get_staleness() = bg_create_table_msg.get_staleness();
      create_table_msg.get_row_type() = bg_create_table_msg.get_row_type();
      create_table_msg.get_row_capacity()
	= bg_create_table_msg.get_row_capacity();
      create_table_msg.get_oplog_dense_serialized()
          = bg_create_table_msg.get_oplog_dense_serialized();
      create_table_msg.get_row_oplog_type()
          = bg_create_table_msg.get_row_oplog_type();
      create_table_msg.get_dense_row_oplog_capacity()
          = bg_create_table_msg.get_dense_row_oplog_capacity();

      table_id = create_table_msg.get_table_id();

      // send msg to name node
      int32_t name_node_id = GlobalContext::get_name_node_id();
      size_t sent_size = (comm_bus_->*(comm_bus_->SendAny_))(name_node_id,
	create_table_msg.get_mem(), create_table_msg.get_size());
      CHECK_EQ(sent_size, create_table_msg.get_size());
    }

    // wait for response from name node
    {
      zmq::message_t zmq_msg;
      int32_t name_node_id;
      (comm_bus_->*(comm_bus_->RecvAny_))(&name_node_id, &zmq_msg);
      MsgType msg_type = MsgBase::get_msg_type(zmq_msg.data());

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

      size_t sent_size = comm_bus_->SendInProc(sender_id, zmq_msg.data(),
        zmq_msg.size());
      CHECK_EQ(sent_size, zmq_msg.size());
    }
  }

  {
    zmq::message_t zmq_msg;
    int32_t sender_id;
    (comm_bus_->*(comm_bus_->RecvAny_))(&sender_id, &zmq_msg);
    MsgType msg_type = MsgBase::get_msg_type(zmq_msg.data());
    CHECK_EQ(msg_type, kCreatedAllTables);
  }
}

long AbstractBgWorker::HandleClockMsg(bool clock_advanced) {
  STATS_BG_ACCUM_CLOCK_END_OPLOG_SERIALIZE_BEGIN();
  BgOpLog *bg_oplog = PrepareOpLogsToSend();

  CreateOpLogMsgs(bg_oplog);
  STATS_BG_ACCUM_CLOCK_END_OPLOG_SERIALIZE_END();

  clock_has_pushed_ = client_clock_;

  SendOpLogMsgs(clock_advanced);
  TrackBgOpLog(bg_oplog);
  return 0;
}

void AbstractBgWorker::HandleServerPushRow(int32_t sender_id, void *msg_mem) {
  LOG(FATAL) << "Consistency model = " << GlobalContext::get_consistency_model()
             << " does not support HandleServerPushRow";
}

void AbstractBgWorker::PrepareBeforeInfiniteLoop() { }

void AbstractBgWorker::FinalizeTableStats() { }

long AbstractBgWorker::ResetBgIdleMilli() {
  return 0;
}

long AbstractBgWorker::BgIdleWork() {
  return 0;
}

void AbstractBgWorker::HandleAppendOpLogMsg(int32_t table_id) {
  STATS_BG_ACCUM_HANDLE_APPEND_OPLOG_BEGIN();
  auto table_iter = tables_->find(table_id);
  CHECK(table_iter != tables_->end());

  AbstractAppendOnlyBuffer *buff
      = table_iter->second->get_oplog().GetAppendOnlyBuffer(my_comm_channel_idx_);

  if (table_iter->second->get_bg_apply_append_oplog_freq() > 0) {
    HandleAppendOpLogAndApply(table_id, table_iter->second, buff);
  } else {
    HandleAppendOpLogAndNotApply(table_id, table_iter->second, buff);
  }

  buff->ResetSize();

  table_iter->second->get_oplog().PutBackBuffer(my_comm_channel_idx_, buff);
  STATS_BG_ACCUM_HANDLE_APPEND_OPLOG_END();
}

AppendOnlyRowOpLogBuffer *AbstractBgWorker::CreateAppendOnlyRowOpLogBufferIfNotExist(
    int32_t table_id, ClientTable *table) {

  auto buff_iter = append_only_row_oplog_buffer_map_.find(table_id);
  if (buff_iter == append_only_row_oplog_buffer_map_.end()) {
    AppendOnlyRowOpLogBuffer *append_only_row_oplog_buffer
        = new  AppendOnlyRowOpLogBuffer(
            table->get_row_oplog_type(),
            table->get_sample_row(),
            table->get_sample_row()->get_update_size(),
            table->get_dense_row_oplog_capacity(),
            table->get_append_only_oplog_type());
    append_only_row_oplog_buffer_map_.insert(
        std::make_pair(table_id, append_only_row_oplog_buffer));
    buff_iter = append_only_row_oplog_buffer_map_.find(table_id);
  }

  return buff_iter->second;
}

void AbstractBgWorker::HandleAppendOpLogAndApply(
    int32_t table_id, ClientTable *table, AbstractAppendOnlyBuffer *buff) {

  AppendOnlyRowOpLogBuffer *append_only_row_oplog_buffer
      = CreateAppendOnlyRowOpLogBufferIfNotExist(table_id, table);
  {
    int32_t row_id, num_updates;
    const int32_t *col_ids;
    buff->InitRead();
    const void *updates = buff->Next(&row_id, &col_ids, &num_updates);
    while (updates != 0) {
      append_only_row_oplog_buffer->BatchIncTmp(row_id, col_ids,
                                               updates, num_updates);
      updates = buff->Next(&row_id, &col_ids, &num_updates);
    }
  }

  auto count_iter = append_only_buff_proc_count_.find(table_id);
  if (count_iter == append_only_buff_proc_count_.end()) {
    append_only_buff_proc_count_.insert(std::make_pair(table_id, 0));
    count_iter = append_only_buff_proc_count_.find(table_id);
  }

  ++(count_iter->second);
  if (count_iter->second % table->get_bg_apply_append_oplog_freq() == 0) {
    AbstractProcessStorage &process_storage = table->get_process_storage();
    int32_t row_id;
    AbstractRowOpLog *row_oplog
        = append_only_row_oplog_buffer->InitReadTmpOpLog(&row_id);
    while (row_oplog != 0) {
      RowAccessor row_accessor;
      ClientRow *client_row = process_storage.Find(row_id, &row_accessor);
      if (client_row != 0) {
        AbstractRow *row_data = client_row->GetRowDataPtr();
        row_data->GetWriteLock();
        int32_t column_id;
        const void *update;
        update = row_oplog->BeginIterateConst(&column_id);
        while (update != 0) {
          row_data->ApplyIncUnsafe(column_id, update);
          update = row_oplog->NextConst(&column_id);
        }
        row_data->ReleaseWriteLock();
      }
      row_oplog = append_only_row_oplog_buffer->NextReadTmpOpLog(&row_id);
    }
    count_iter->second = 0;
    append_only_row_oplog_buffer->MergeTmpOpLog();
  }
}

void AbstractBgWorker::HandleAppendOpLogAndNotApply(
    int32_t table_id, ClientTable *table, AbstractAppendOnlyBuffer *buff) {
  AppendOnlyRowOpLogBuffer *append_only_row_oplog_buffer
      = CreateAppendOnlyRowOpLogBufferIfNotExist(table_id, table);

  {
    int32_t row_id, num_updates;
    const int32_t *col_ids;
    buff->InitRead();
    const void *updates = buff->Next(&row_id, &col_ids, &num_updates);
    while (updates != 0) {
      append_only_row_oplog_buffer->BatchInc(row_id, col_ids,
                                             updates, num_updates);
      updates = buff->Next(&row_id, &col_ids, &num_updates);
    }
  }
}

void AbstractBgWorker::FinalizeOpLogMsgStats(
    int32_t table_id,
    std::map<int32_t, size_t> *table_num_bytes_by_server,
    std::map<int32_t, std::map<int32_t, size_t> >
    *server_table_oplog_size_map) {
  for (auto server_iter = (*table_num_bytes_by_server).begin();
       server_iter != (*table_num_bytes_by_server).end(); ++server_iter) {
    // 1. int32_t: number of rows
    if (server_iter->second != 0)
      server_iter->second += sizeof(int32_t);
  }

  for (auto server_iter = (*table_num_bytes_by_server).begin();
       server_iter != (*table_num_bytes_by_server).end(); server_iter++) {
    if (server_iter->second == 0)
      (*server_table_oplog_size_map)[server_iter->first].erase(table_id);
    else
      (*server_table_oplog_size_map)[server_iter->first][table_id]
          = server_iter->second;
  }
}

void AbstractBgWorker::CreateOpLogMsgs(const BgOpLog *bg_oplog) {
  std::map<int32_t, std::map<int32_t, void*> > table_server_mem_map;

  for (auto server_iter = server_table_oplog_size_map_.begin();
    server_iter != server_table_oplog_size_map_.end(); server_iter++) {
    OpLogSerializer oplog_serializer;
    int32_t server_id = server_iter->first;
    size_t server_oplog_msg_size
        = oplog_serializer.Init(server_iter->second);

    if (server_oplog_msg_size == 0) {
        server_oplog_msg_map_.erase(server_id);
      continue;
    }

    server_oplog_msg_map_[server_id]
      = new ClientSendOpLogMsg(server_oplog_msg_size);

    oplog_serializer.AssignMem(server_oplog_msg_map_[server_id]->get_data());

    for (const auto &table_pair : (*tables_)) {
      int32_t table_id = table_pair.first;
      uint8_t *table_ptr
	= reinterpret_cast<uint8_t*>(oplog_serializer.GetTablePtr(table_id));

      if (table_ptr == 0) {
        table_server_mem_map[table_id].erase(server_id);
        continue;
      }

      // table id
      *(reinterpret_cast<int32_t*>(table_ptr)) = table_id;

      // table update size
      *(reinterpret_cast<size_t*>(table_ptr + sizeof(int32_t)))
	= table_pair.second->get_sample_row()->get_update_size();

      // offset for table rows
      table_server_mem_map[table_id][server_id]
        = table_ptr + sizeof(int32_t) + sizeof(size_t);
    }
  }

  for (const auto &table_pair : (*tables_)) {
    int32_t table_id = table_pair.first;
    ClientTable *table = table_pair.second;
    if (table->get_no_oplog_replay()) {
      auto serializer_iter = row_oplog_serializer_map_.find(table_id);
      CHECK(serializer_iter != row_oplog_serializer_map_.end());

      RowOpLogSerializer *row_oplog_serializer = serializer_iter->second;
      row_oplog_serializer->SerializeByServer(&(table_server_mem_map[table_id]));
    } else {
      BgOpLogPartition *oplog_partition = bg_oplog->Get(table_id);
      oplog_partition->SerializeByServer(
          &(table_server_mem_map[table_id]),
          table_pair.second->oplog_dense_serialized());
    }
  }
}

size_t AbstractBgWorker::SendOpLogMsgs(bool clock_advanced) {
  size_t accum_size = 0;
  for (const auto &server_id : server_ids_) {
    auto oplog_msg_iter = server_oplog_msg_map_.find(server_id);
    if (oplog_msg_iter != server_oplog_msg_map_.end()) {
      oplog_msg_iter->second->get_is_clock() = clock_advanced;
      oplog_msg_iter->second->get_client_id() = GlobalContext::get_client_id();
      oplog_msg_iter->second->get_version() = version_;
      oplog_msg_iter->second->get_bg_clock() = clock_has_pushed_ + 1;

      accum_size += oplog_msg_iter->second->get_size();
      MemTransfer::TransferMem(comm_bus_, server_id, oplog_msg_iter->second);
      // delete message after send
      delete oplog_msg_iter->second;
      oplog_msg_iter->second = 0;
    } else {
      ClientSendOpLogMsg clock_oplog_msg(0);
      clock_oplog_msg.get_is_clock() = clock_advanced;
      clock_oplog_msg.get_client_id() = GlobalContext::get_client_id();
      clock_oplog_msg.get_version() = version_;
      clock_oplog_msg.get_bg_clock() = clock_has_pushed_ + 1;

      accum_size += clock_oplog_msg.get_size();
      MemTransfer::TransferMem(comm_bus_, server_id, &clock_oplog_msg);
    }
  }

  STATS_BG_ADD_PER_CLOCK_OPLOG_SIZE(accum_size);

  return accum_size;
}

size_t AbstractBgWorker::CountRowOpLogToSend(
      int32_t row_id, AbstractRowOpLog *row_oplog,
      std::map<int32_t, size_t> *table_num_bytes_by_server,
      BgOpLogPartition *bg_table_oplog,
      GetSerializedRowOpLogSizeFunc GetSerializedRowOpLogSize) {

  // update oplog message size
  int32_t server_id = GlobalContext::GetPartitionServerID(
      row_id, my_comm_channel_idx_);
  // 1) row id
  // 2) serialized row size
  size_t serialized_size = sizeof(int32_t)
                           + GetSerializedRowOpLogSize(row_oplog);
  (*table_num_bytes_by_server)[server_id] += serialized_size;
  bg_table_oplog->InsertOpLog(row_id, row_oplog);
  return serialized_size;
}

void AbstractBgWorker::RecvAppInitThreadConnection(int32_t *num_connected_app_threads) {
  zmq::message_t zmq_msg;
  int32_t sender_id;
  comm_bus_->RecvInProc(&sender_id, &zmq_msg);
  MsgType msg_type = MsgBase::get_msg_type(zmq_msg.data());
  CHECK_EQ(msg_type, kAppConnect) << "send_id = " << sender_id;
  ++(*num_connected_app_threads);
  CHECK(*num_connected_app_threads <= GlobalContext::get_num_app_threads());
}

void AbstractBgWorker::CheckForwardRowRequestToServer(
    int32_t app_thread_id, RowRequestMsg &row_request_msg) {

  int32_t table_id = row_request_msg.get_table_id();
  int32_t row_id = row_request_msg.get_row_id();
  int32_t clock = row_request_msg.get_clock();
  bool forced = row_request_msg.get_forced_request();

  if (!forced) {
    // Check if the row exists in process cache
    auto table_iter = tables_->find(table_id);
    CHECK(table_iter != tables_->end());
    {
      // check if it is in process storage
      ClientTable *table = table_iter->second;
      AbstractProcessStorage &table_storage = table->get_process_storage();
      RowAccessor row_accessor;
      ClientRow *client_row = table_storage.Find(row_id, &row_accessor);
      if (client_row != 0) {
        if ((GlobalContext::get_consistency_model() == SSP
             && client_row->GetClock() >= clock)
            || (GlobalContext::get_consistency_model() == SSPPush)
            || (GlobalContext::get_consistency_model() == SSPAggr)) {
          RowRequestReplyMsg row_request_reply_msg;
          size_t sent_size = comm_bus_->SendInProc(
              app_thread_id, row_request_reply_msg.get_mem(),
              row_request_reply_msg.get_size());
          CHECK_EQ(sent_size, row_request_reply_msg.get_size());
          return;
        }
      }
    }
  }

  std::pair<int32_t, int32_t> request_key(table_id, row_id);
  RowRequestInfo row_request;
  row_request.app_thread_id = app_thread_id;
  row_request.clock = row_request_msg.get_clock();

  // Version in request denotes the update version that the row on server can
  // see. Which should be 1 less than the current version number.
  row_request.version = version_ - 1;

  bool should_be_sent
    = row_request_oplog_mgr_->AddRowRequest(row_request, table_id, row_id);

  if (should_be_sent) {
    int32_t server_id
        = GlobalContext::GetPartitionServerID(row_id, my_comm_channel_idx_);

    size_t sent_size = (comm_bus_->*(comm_bus_->SendAny_))(server_id,
      row_request_msg.get_mem(), row_request_msg.get_size());
    CHECK_EQ(sent_size, row_request_msg.get_size());
  }
}

void AbstractBgWorker::UpdateExistingRow(
    int32_t table_id,
    int32_t row_id, ClientRow *client_row, ClientTable *client_table,
    const void *data, size_t row_size, uint32_t version) {
  AbstractRow *row_data = client_row->GetRowDataPtr();
  if (client_table->get_oplog_type() == Sparse
      || client_table->get_oplog_type() == Dense) {
    AbstractOpLog &table_oplog = client_table->get_oplog();
    OpLogAccessor oplog_accessor;
    bool oplog_found = table_oplog.FindAndLock(row_id, &oplog_accessor);

    row_data->GetWriteLock();
    row_data->ResetRowData(data, row_size);

    bool no_oplog_replay = client_table->get_no_oplog_replay();

    if (!no_oplog_replay)
      CheckAndApplyOldOpLogsToRowData(table_id, row_id, version, row_data);

    if (oplog_found && !no_oplog_replay) {
      STATS_BG_ACCUM_SERVER_PUSH_OPLOG_ROW_APPLIED_ADD_ONE();
      int32_t column_id;
      const void *update;
      update = oplog_accessor.get_row_oplog()->BeginIterateConst(&column_id);
      while (update != 0) {
        STATS_BG_ACCUM_SERVER_PUSH_UPDATE_APPLIED_ADD_ONE();
        row_data->ApplyIncUnsafe(column_id, update);
        update = oplog_accessor.get_row_oplog()->NextConst(&column_id);
      }
    }
    row_data->ReleaseWriteLock();
  } else if (client_table->get_oplog_type() == AppendOnly) {
    row_data->GetWriteLock();
    row_data->ResetRowData(data, row_size);
    auto buff_iter = append_only_row_oplog_buffer_map_.find(table_id);
    if (buff_iter != append_only_row_oplog_buffer_map_.end()) {
      AppendOnlyRowOpLogBuffer *append_only_row_oplog_buffer = buff_iter->second;

      AbstractRowOpLog *row_oplog
          = append_only_row_oplog_buffer->GetRowOpLog(row_id);

      if (row_oplog != 0) {
        int32_t column_id;
        const void *update;
        update = row_oplog->BeginIterateConst(&column_id);
        while (update != 0) {
          row_data->ApplyIncUnsafe(column_id, update);
          update = row_oplog->NextConst(&column_id);
        }
      }
    }
    row_data->ReleaseWriteLock();
  } else {
    LOG(FATAL) << "Unknown oplog type " << client_table->get_oplog_type();
  }
}

void AbstractBgWorker::InsertNonexistentRow(int32_t table_id, int32_t row_id,
                                            ClientTable *client_table, const void *data,
                                            size_t row_size, uint32_t version,
                                            int32_t clock) {
  int32_t row_type = client_table->get_row_type();
  AbstractRow *row_data
      = ClassRegistry<AbstractRow>::GetRegistry().CreateObject(row_type);

  row_data->Deserialize(data, row_size);

  bool no_oplog_replay = client_table->get_no_oplog_replay();
  if (!no_oplog_replay)
    CheckAndApplyOldOpLogsToRowData(table_id, row_id, version, row_data);

  ClientRow *client_row = CreateClientRow(clock, row_data);
  if (client_table->get_oplog_type() == Sparse ||
      client_table->get_oplog_type() == Dense) {
    AbstractOpLog &table_oplog = client_table->get_oplog();
    OpLogAccessor oplog_accessor;
    bool oplog_found = table_oplog.FindAndLock(row_id, &oplog_accessor);

    if (oplog_found && !no_oplog_replay) {
      int32_t column_id;
      const void *update;
      update = oplog_accessor.get_row_oplog()->BeginIterateConst(&column_id);
      while (update != 0) {
        row_data->ApplyIncUnsafe(column_id, update);
        update = oplog_accessor.get_row_oplog()->NextConst(&column_id);
      }
    }
    client_table->get_process_storage().Insert(row_id, client_row);
  } else if (client_table->get_oplog_type() == AppendOnly) { //AppendOnly
    auto buff_iter = append_only_row_oplog_buffer_map_.find(table_id);
    if (buff_iter != append_only_row_oplog_buffer_map_.end()) {
      AppendOnlyRowOpLogBuffer *append_only_row_oplog_buffer = buff_iter->second;

      AbstractRowOpLog *row_oplog
          = append_only_row_oplog_buffer->GetRowOpLog(row_id);

      if (row_oplog != 0) {
        int32_t column_id;
        const void *update;
        update = row_oplog->BeginIterateConst(&column_id);
        while (update != 0) {
          row_data->ApplyIncUnsafe(column_id, update);
          update = row_oplog->NextConst(&column_id);
        }
      }
    }
    client_table->get_process_storage().Insert(row_id, client_row);
  } else {
    LOG(FATAL) << "Unkonwn oplog type = " << client_table->get_oplog_type();
  }
}

void AbstractBgWorker::HandleServerRowRequestReply(
    int32_t server_id,
    ServerRowRequestReplyMsg &server_row_request_reply_msg) {

  int32_t table_id = server_row_request_reply_msg.get_table_id();
  int32_t row_id = server_row_request_reply_msg.get_row_id();
  int32_t clock = server_row_request_reply_msg.get_clock();
  uint32_t version = server_row_request_reply_msg.get_version();

  auto table_iter = tables_->find(table_id);
  CHECK(table_iter != tables_->end()) << "Cannot find table " << table_id;
  ClientTable *client_table = table_iter->second;

  row_request_oplog_mgr_->ServerAcknowledgeVersion(server_id, version);

  RowAccessor row_accessor;
  ClientRow *client_row = client_table->get_process_storage().Find(
      row_id, &row_accessor);

  const void *data = server_row_request_reply_msg.get_row_data();
  size_t row_size = server_row_request_reply_msg.get_row_size();

  if (client_row != 0) {
    UpdateExistingRow(table_id, row_id, client_row, client_table, data,
                      row_size, version);
    client_row->SetClock(clock);
  } else { // not found
    InsertNonexistentRow(table_id, row_id, client_table, data, row_size, version, clock);
  }

  std::vector<int32_t> app_thread_ids;
  int32_t clock_to_request
    = row_request_oplog_mgr_->InformReply(
        table_id, row_id, clock, version_, &app_thread_ids);

  if (clock_to_request >= 0) {
    RowRequestMsg row_request_msg;
    row_request_msg.get_table_id() = table_id;
    row_request_msg.get_row_id() = row_id;
    row_request_msg.get_clock() = clock_to_request;

    int32_t server_id = GlobalContext::GetPartitionServerID(
        row_id, my_comm_channel_idx_);

    size_t sent_size = (comm_bus_->*(comm_bus_->SendAny_))(server_id,
      row_request_msg.get_mem(), row_request_msg.get_size());
    CHECK_EQ(sent_size, row_request_msg.get_size());
  }

  std::pair<int32_t, int32_t> request_key(table_id, row_id);
  RowRequestReplyMsg row_request_reply_msg;

  for (int i = 0; i < (int) app_thread_ids.size(); ++i) {
    size_t sent_size = comm_bus_->SendInProc(app_thread_ids[i],
      row_request_reply_msg.get_mem(), row_request_reply_msg.get_size());
    CHECK_EQ(sent_size, row_request_reply_msg.get_size());
  }
}

size_t AbstractBgWorker::SendMsg(MsgBase *msg) {
  size_t sent_size = comm_bus_->SendInProc(my_id_, msg->get_mem(),
                                            msg->get_size());
  return sent_size;
}

void AbstractBgWorker::RecvMsg(zmq::message_t &zmq_msg) {
  int32_t sender_id;
  comm_bus_->RecvInProc(&sender_id, &zmq_msg);
}

void AbstractBgWorker::ConnectToNameNodeOrServer(int32_t server_id) {
  ClientConnectMsg client_connect_msg;
  client_connect_msg.get_client_id() = GlobalContext::get_client_id();
  void *msg = client_connect_msg.get_mem();
  int32_t msg_size = client_connect_msg.get_size();

  if (comm_bus_->IsLocalEntity(server_id)) {
    comm_bus_->ConnectTo(server_id, msg, msg_size);
  } else {
    HostInfo server_info;
    if (server_id == GlobalContext::get_name_node_id())
      server_info = GlobalContext::get_name_node_info();
    else
      server_info = GlobalContext::get_server_info(server_id);

    std::string server_addr = server_info.ip + ":" + server_info.port;
    comm_bus_->ConnectTo(server_id, server_addr, msg, msg_size);
  }
}

void *AbstractBgWorker::operator() () {
  STATS_REGISTER_THREAD(kBgThread);

  ThreadContext::RegisterThread(my_id_);

  NumaMgr::ConfigureBgWorker();

  InitCommBus();

  BgServerHandshake();

  pthread_barrier_wait(init_barrier_);

  int32_t num_connected_app_threads = 0;
  int32_t num_deregistered_app_threads = 0;
  int32_t num_shutdown_acked_servers = 0;

  RecvAppInitThreadConnection(&num_connected_app_threads);

  if(my_comm_channel_idx_ == 0){
    HandleCreateTables();
  }
  pthread_barrier_wait(create_table_barrier_);

  FinalizeTableStats();

  zmq::message_t zmq_msg;
  int32_t sender_id;
  MsgType msg_type;
  void *msg_mem;
  bool destroy_mem = false;
  long timeout_milli = GlobalContext::get_bg_idle_milli();
  PrepareBeforeInfiniteLoop();
  while (1) {
    bool received = WaitMsg_(&sender_id, &zmq_msg, timeout_milli);

    if (!received) {
      timeout_milli = BgIdleWork();
      continue;
    } else {
      // (TODO): verify the difference in performance
      timeout_milli = ResetBgIdleMilli();
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
            (comm_bus_->*(comm_bus_->SendAny_))(name_node_id, msg.get_mem(),
              msg.get_size());

            for (const auto &server_id : server_ids_) {
              (comm_bus_->*(comm_bus_->SendAny_))(server_id, msg.get_mem(),
                                           msg.get_size());
            }
          }
        }
        break;
      case kServerShutDownAck:
        {
          ++num_shutdown_acked_servers;
          if (num_shutdown_acked_servers
              == GlobalContext::get_num_clients() + 1) {
	    comm_bus_->ThreadDeregister();
            STATS_DEREGISTER_THREAD();
	    return 0;
          }
        }
      break;
      case kRowRequest:
        {
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
          timeout_milli = HandleClockMsg(true);
          ++client_clock_;
          STATS_BG_CLOCK();
        }
        break;
      case kBgSendOpLog:
        {
          timeout_milli = HandleClockMsg(false);
        }
        break;
      case kServerPushRow:
        {
          HandleServerPushRow(sender_id, msg_mem);
        }
        break;
      case kServerOpLogAck:
        {
          ServerOpLogAckMsg server_oplog_ack_msg(msg_mem);
          row_request_oplog_mgr_->ServerAcknowledgeVersion(
              sender_id, server_oplog_ack_msg.get_ack_version());
        }
        break;
      case kBgHandleAppendOpLog:
        {
          BgHandleAppendOpLogMsg handle_append_oplog_msg(msg_mem);
          HandleAppendOpLogMsg(handle_append_oplog_msg.get_table_id());
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

}
