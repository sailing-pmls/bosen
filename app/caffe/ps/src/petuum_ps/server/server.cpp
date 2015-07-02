// author: jinliang

#include <petuum_ps/server/server.hpp>
#include <petuum_ps/server/serialized_oplog_reader.hpp>
#include <petuum_ps_common/util/class_register.hpp>
#include <petuum_ps_common/util/stats.hpp>

#include <utility>
#include <fstream>
#include <map>

namespace petuum {

Server::Server() {}

Server::~Server() {}

void Server::Init(int32_t server_id,
                  const std::vector<int32_t> &bg_ids,
                  MsgTracker *msg_tracker) {
  for (auto iter = bg_ids.cbegin(); iter != bg_ids.cend(); iter++){
    bg_clock_.AddClock(*iter, 0);
    bg_version_map_[*iter] = -1;
  }
   push_row_msg_data_size_ = kPushRowMsgSizeInit;

   server_id_ = server_id;

   accum_oplog_count_ = 0;
   msg_tracker_ = msg_tracker;
 }

 void Server::CreateTable(int32_t table_id, TableInfo &table_info){
   auto ret = tables_.emplace(table_id, ServerTable(table_id, table_info));
   CHECK(ret.second);

   if (GlobalContext::get_resume_clock() > 0) {
     boost::unordered_map<int32_t, ServerTable>::iterator table_iter
         = tables_.find(table_id);
     table_iter->second.ReadSnapShot(GlobalContext::get_resume_dir(),
                                     server_id_, table_id,
                                     GlobalContext::get_resume_clock());
   }
 }

 ServerRow *Server::FindCreateRow(int32_t table_id, int32_t row_id){
   // access ServerTable via reference to avoid copying
   auto iter = tables_.find(table_id);
   CHECK(iter != tables_.end());

   ServerTable &server_table = iter->second;
   ServerRow *server_row = server_table.FindRow(row_id);
   if(server_row != 0) {
     return server_row;
   }

   server_row = server_table.CreateRow(row_id);

   return server_row;
 }

 bool Server::ClockUntil(int32_t bg_id, int32_t clock) {
   int new_clock = bg_clock_.TickUntil(bg_id, clock);
   if(new_clock) {
     if (GlobalContext::get_snapshot_clock() <= 0
         || new_clock % GlobalContext::get_snapshot_clock() != 0)
       return true;
     for (auto table_iter = tables_.begin(); table_iter != tables_.end();
          table_iter++) {
       //LOG(INFO) << "Take snapshot - new clock = " << new_clock;
       table_iter->second.TakeSnapShot(GlobalContext::get_snapshot_dir(),
                                       server_id_,
                                       table_iter->first, new_clock);
     }
     return true;
   }

   return false;
 }

 void Server::AddRowRequest(int32_t bg_id, int32_t table_id, int32_t row_id,
   int32_t clock) {

   ServerRowRequest server_row_request;
   server_row_request.bg_id = bg_id;
   server_row_request.table_id = table_id;
   server_row_request.row_id = row_id;
   server_row_request.clock = clock;

   if (clock_bg_row_requests_.count(clock) == 0) {
     clock_bg_row_requests_.insert(std::make_pair(clock,
       boost::unordered_map<int32_t, std::vector<ServerRowRequest> >()));
   }
   if (clock_bg_row_requests_[clock].count(bg_id) == 0) {
     clock_bg_row_requests_[clock].insert(std::make_pair(bg_id,
       std::vector<ServerRowRequest>()));
   }
   clock_bg_row_requests_[clock][bg_id].push_back(server_row_request);
 }

 void Server::GetFulfilledRowRequests(std::vector<ServerRowRequest> *requests) {
   int32_t clock = bg_clock_.get_min_clock();
   requests->clear();
   auto iter = clock_bg_row_requests_.find(clock);

   if(iter == clock_bg_row_requests_.end())
     return;
   boost::unordered_map<int32_t,
     std::vector<ServerRowRequest> > &bg_row_requests = iter->second;

   for (auto bg_iter = bg_row_requests.begin(); bg_iter != bg_row_requests.end();
     bg_iter++) {
     requests->insert(requests->end(), bg_iter->second.begin(),
       bg_iter->second.end());
   }

   clock_bg_row_requests_.erase(clock);
 }

 void Server::ApplyOpLogUpdateVersion(
     const void *oplog, size_t oplog_size, int32_t bg_thread_id,
     uint32_t version) {

   CHECK_EQ(bg_version_map_[bg_thread_id] + 1, version)
       << "bg_thread_id = " << bg_thread_id;
   bg_version_map_[bg_thread_id] = version;

   if (oplog_size == 0) return;

   SerializedOpLogReader oplog_reader(oplog, tables_);
   bool to_read = oplog_reader.Restart();

   //LOG(INFO) << "oplog size = " << oplog_size
   //        << " to read = " << to_read;

   if(!to_read) return;

   int32_t table_id;
   int32_t row_id;
   const int32_t * column_ids; // the variable pointer points to const memory
   int32_t num_updates;
   bool started_new_table;
   const void *updates = oplog_reader.Next(&table_id, &row_id, &column_ids,
                                           &num_updates, &started_new_table);

   ServerTable *server_table;
   if (updates != 0) {
     auto table_iter = tables_.find(table_id);
     CHECK(table_iter != tables_.end())
         << "Not found table_id = " << table_id;
     server_table = &(table_iter->second);
   }

   while (updates != 0) {
     ++accum_oplog_count_;
     //LOG(INFO) << "table_id = " << table_id
     //        << " row_id = " << row_id
     //        << " updates = " << updates
     //        << " num_updates = " << num_updates;
     bool found
       = server_table->ApplyRowOpLog(row_id, column_ids, updates, num_updates);

     if (!found) {
       server_table->CreateRow(row_id);
       server_table->ApplyRowOpLog(row_id, column_ids, updates, num_updates);
     }

     updates = oplog_reader.Next(&table_id, &row_id, &column_ids,
                                 &num_updates, &started_new_table);

     if (updates == 0) break;
     if (started_new_table) {
       auto table_iter = tables_.find(table_id);
       CHECK(table_iter != tables_.end())
         << "Not found table_id = " << table_id;
       server_table = &(table_iter->second);
     }
   }
 }

 int32_t Server::GetMinClock() {
   return bg_clock_.get_min_clock();
 }

 int32_t Server::GetBgVersion(int32_t bg_thread_id) {
   return bg_version_map_[bg_thread_id];
 }

 size_t Server::CreateSendServerPushRowMsgs(PushMsgSendFunc PushMsgSend,
                                            bool clock_changed) {
   boost::unordered_map<int32_t, RecordBuff> buffs;
   boost::unordered_map<int32_t, ServerPushRowMsg*> msg_map;

   accum_oplog_count_ = 0;

   size_t accum_send_bytes = 0;

   int32_t comm_channel_idx
       = GlobalContext::GetCommChannelIndexServer(server_id_);

   // Create a message for each bg thread
   int32_t client_id = 0;
   for (client_id = 0;
        client_id < GlobalContext::get_num_clients(); ++client_id) {
    ServerPushRowMsg *msg = new ServerPushRowMsg(push_row_msg_data_size_);
    msg_map[client_id] = msg;
    buffs.insert(
        std::make_pair(client_id,
                       RecordBuff(msg->get_data(), push_row_msg_data_size_)));
  }

  int32_t num_tables_left = GlobalContext::get_num_tables();
  for (auto table_iter = tables_.begin(); table_iter != tables_.end();
       table_iter++) {
    int32_t table_id = table_iter->first;

    STATS_SERVER_ACCUM_IMPORTANCE(table_id, 0.0, false);

    ServerTable &server_table = table_iter->second;
    for (client_id = 0;
         client_id < GlobalContext::get_num_clients(); ++client_id) {
      RecordBuff &record_buff = buffs[client_id];
      int32_t *table_id_ptr = record_buff.GetMemPtrInt32();
      if (table_id_ptr == 0) {
        int32_t bg_id = GlobalContext::get_bg_thread_id(client_id,
                                                        comm_channel_idx);
        PushMsgSend(bg_id, msg_map[client_id], false && clock_changed,
                    GetBgVersion(bg_id), GetMinClock(), msg_tracker_);
        memset((msg_map[client_id])->get_data(), 0, push_row_msg_data_size_);
        record_buff.ResetOffset();
        table_id_ptr = record_buff.GetMemPtrInt32();
      }
      *table_id_ptr = table_id;
    }

    // ServerTable packs the data.
    server_table.InitAppendTableToBuffs();
    int32_t failed_client_id;
    size_t num_clients = 0;
    bool pack_suc = server_table.AppendTableToBuffs(
        0, &buffs, &failed_client_id, false,
        &num_clients);

    while (!pack_suc) {
      RecordBuff &record_buff = buffs[failed_client_id];
      int32_t *buff_end_ptr = record_buff.GetMemPtrInt32();
      if (buff_end_ptr != 0)
        *buff_end_ptr = GlobalContext::get_serialized_table_end();

      int32_t bg_id = GlobalContext::get_bg_thread_id(failed_client_id,
                                                      comm_channel_idx);
      PushMsgSend(bg_id, msg_map[failed_client_id], false && clock_changed,
                  GetBgVersion(bg_id), GetMinClock(), msg_tracker_);
      memset((msg_map[failed_client_id])->get_data(), 0,
             push_row_msg_data_size_);
      record_buff.ResetOffset();
      int32_t *table_id_ptr = record_buff.GetMemPtrInt32();
      *table_id_ptr = table_id;
      pack_suc = server_table.AppendTableToBuffs(
          failed_client_id, &buffs, &failed_client_id, true,
          &num_clients);
    }

    --num_tables_left;
    if (num_tables_left > 0) {
      for (client_id = 0;
           client_id < GlobalContext::get_num_clients(); ++client_id) {

        RecordBuff &record_buff = buffs[client_id];
        int32_t *table_sep_ptr = record_buff.GetMemPtrInt32();
        if (table_sep_ptr == 0) {
          int32_t bg_id = GlobalContext::get_bg_thread_id(client_id,
                                                          comm_channel_idx);

          PushMsgSend(bg_id, msg_map[client_id], false && clock_changed,
                      GetBgVersion(bg_id), GetMinClock(), msg_tracker_);
          memset((msg_map[client_id])->get_data(), 0, push_row_msg_data_size_);
          record_buff.ResetOffset();
        } else {
          *table_sep_ptr = GlobalContext::get_serialized_table_separator();
        }
      }
    } else
      break;
  }

  for (client_id = 0;
       client_id < GlobalContext::get_num_clients(); ++client_id) {
    RecordBuff &record_buff = buffs[client_id];
    int32_t *table_end_ptr = record_buff.GetMemPtrInt32();
    if (table_end_ptr == 0) {
      int32_t bg_id = GlobalContext::get_bg_thread_id(client_id,
                                                      comm_channel_idx);
      PushMsgSend(bg_id, msg_map[client_id], true && clock_changed,
                  GetBgVersion(bg_id), GetMinClock(), msg_tracker_);
      continue;
    }
    *table_end_ptr = GlobalContext::get_serialized_table_end();
    msg_map[client_id]->get_avai_size() = buffs[client_id].GetMemUsedSize();
    accum_send_bytes += msg_map[client_id]->get_size();

    int32_t bg_id = GlobalContext::get_bg_thread_id(client_id,
                                                    comm_channel_idx);
    PushMsgSend(bg_id, msg_map[client_id], true && clock_changed,
                GetBgVersion(bg_id), GetMinClock(), msg_tracker_);
    delete msg_map[client_id];
  }
  return accum_send_bytes;
}

size_t Server::CreateSendServerPushRowMsgsPartial(
    PushMsgSendFunc PushMsgSend) {
  boost::unordered_map<int32_t, RecordBuff> buffs;
  boost::unordered_map<int32_t, ServerPushRowMsg*> msg_map;
  boost::unordered_map<int32_t, size_t> client_buff_size;
  boost::unordered_map<int32_t,
                       std::vector<std::pair<int32_t, ServerRow*> > >
      table_rows_to_send;

  bool has_to_send = false;
  accum_oplog_count_ = 0;

  size_t accum_send_bytes = 0;

  int32_t comm_channel_idx
      = GlobalContext::GetCommChannelIndexServer(server_id_);

  // Create a message for each bg thread
  int32_t client_id = 0;
  for (client_id = 0;
       client_id < GlobalContext::get_num_clients(); ++client_id) {
    client_buff_size[client_id] = 0;
  }

  for (auto &table_pair : tables_) {
    int32_t table_id = table_pair.first;

    table_rows_to_send.insert(std::make_pair(
        table_id,
        std::vector<std::pair<int32_t, ServerRow*> >()));

    table_pair.second.GetPartialTableToSend(
        &(table_rows_to_send[table_id]),
        &client_buff_size);
    if (table_rows_to_send[table_id].size() > 0) has_to_send = true;
  }

  if (!has_to_send) return 0;

  size_t num_tables = tables_.size();

  for (auto buff_size_iter = client_buff_size.begin();
       buff_size_iter != client_buff_size.end(); ++buff_size_iter) {

    if (buff_size_iter->second > 0)
      buff_size_iter->second += (sizeof(int32_t) + sizeof(int32_t))*num_tables;
  }

  for (auto buff_size_iter = client_buff_size.begin();
       buff_size_iter != client_buff_size.end(); buff_size_iter++) {
    size_t buff_size = buff_size_iter->second;
    int32_t client_id = buff_size_iter->first;

    if (buff_size == 0) {
      msg_map[client_id] = 0;
      continue;
    }

    ServerPushRowMsg *msg = new ServerPushRowMsg(buff_size);
    msg_map[client_id] = msg;
    buffs.insert(
        std::make_pair(client_id,
                       RecordBuff(msg->get_data(), buff_size)));
  }

  size_t num_tables_left = tables_.size();

  for (auto table_iter = tables_.begin(); table_iter != tables_.end();
       table_iter++) {
    int32_t table_id = table_iter->first;
    ServerTable &server_table = table_iter->second;

    for (auto buff_iter = buffs.begin(); buff_iter != buffs.end();
         ++buff_iter) {
      RecordBuff &record_buff = buff_iter->second;
      int32_t *table_id_ptr = record_buff.GetMemPtrInt32();
      CHECK_NOTNULL(table_id_ptr);
      *table_id_ptr = table_id;
    }

    server_table.AppendRowsToBuffsPartial(
        &buffs, table_rows_to_send[table_id]);

    --num_tables_left;

    for (auto buff_iter = buffs.begin(); buff_iter != buffs.end();
         ++buff_iter) {
      RecordBuff &record_buff = buff_iter->second;
      int32_t *table_end_ptr = record_buff.GetMemPtrInt32();
      CHECK_NOTNULL(table_end_ptr);

      if (num_tables_left == 0)
        *table_end_ptr = GlobalContext::get_serialized_table_end();
      else
        *table_end_ptr = GlobalContext::get_serialized_table_separator();
    }
  }

  for (auto msg_iter = msg_map.begin(); msg_iter != msg_map.end();
       msg_iter++) {
    int32_t client_id = msg_iter->first;
    ServerPushRowMsg *msg = msg_iter->second;
    if (msg == 0)
      continue;

    accum_send_bytes += msg->get_size();

    int32_t bg_id = GlobalContext::get_bg_thread_id(client_id,
                                                    comm_channel_idx);
    PushMsgSend(bg_id, msg, false, GetBgVersion(bg_id), GetMinClock(), msg_tracker_);

    VLOG(0) << "Send server push row size = " << msg->get_avai_size()
            << " to bg id = " << bg_id
            << " server id = " << ThreadContext::get_id();

    delete msg;
  }

  return accum_send_bytes;
}

bool Server::AccumedOpLogSinceLastPush() {
  return accum_oplog_count_ > 0;
}

void Server::RowSent(int32_t table_id, int32_t row_id,
                     ServerRow *row, size_t num_clients) {
  auto table_iter = tables_.find(table_id);
  CHECK(table_iter != tables_.end());
  table_iter->second.RowSent(row_id, row, num_clients);
}

}  // namespace petuum
