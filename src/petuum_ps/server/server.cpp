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
// author: jinliang

#include <petuum_ps/server/server.hpp>
#include <petuum_ps/oplog/serialized_oplog_reader.hpp>
#include <petuum_ps_common/util/class_register.hpp>

#include <utility>

namespace petuum {

Server::Server() {}

Server::~Server() {}

void Server::AddClientBgPair(int32_t client_id, int32_t bg_id) {
  client_bg_map_[client_id].push_back(bg_id);
  client_ids_.push_back(client_id);
  client_clocks_.AddClock(client_id, 0);
}

void Server::Init(int32_t server_id) {
  for (auto iter = client_bg_map_.begin();
      iter != client_bg_map_.end(); iter++){
    VectorClock vector_clock(iter->second);
    client_vector_clock_map_[iter->first] = vector_clock;

    for (auto bg_iter = iter->second.begin();
	 bg_iter != iter->second.end(); bg_iter++) {
      bg_version_map_[*bg_iter] = -1;
    }
  }
  push_row_msg_data_size_ = kPushRowMsgSizeInit;

  server_id_ = server_id;
}

void Server::CreateTable(int32_t table_id, TableInfo &table_info){
  auto ret = tables_.emplace(table_id, std::move(ServerTable(table_info)));
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
    //VLOG(0) << "Found, return table " << table_id << " row " << row_id;
    return server_row;
  }

  //VLOG(0) << "Create row on read table " << table_id << " row_id " << row_id;
  server_row = server_table.CreateRow(row_id);

  return server_row;
}

bool Server::Clock(int32_t client_id, int32_t bg_id) {
  int new_clock = client_vector_clock_map_[client_id].Tick(bg_id);
  if (new_clock == 0)
    return false;

  new_clock = client_clocks_.Tick(client_id);
  if(new_clock) {
    if (GlobalContext::get_snapshot_clock() <= 0
        || new_clock % GlobalContext::get_snapshot_clock() != 0)
      return true;
    for (auto table_iter = tables_.begin(); table_iter != tables_.end();
         table_iter++) {
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
  int32_t clock = client_clocks_.get_min_clock();
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

void Server::ApplyOpLog(const void *oplog, int32_t bg_thread_id,
  uint32_t version) {

  CHECK_EQ(bg_version_map_[bg_thread_id] + 1, version);
  bg_version_map_[bg_thread_id] = version;

  SerializedOpLogReader oplog_reader(oplog);
  bool to_read = oplog_reader.Restart();

  if(!to_read)
    return;

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
    server_table = &(table_iter->second);
  }

  while (updates != 0) {
    bool found
      = server_table->ApplyRowOpLog(row_id, column_ids, updates, num_updates);
    //VLOG(0) << "Update row_id = " << row_id
    //	    << " num_updates = " << num_updates;
    if (!found) {
      server_table->CreateRow(row_id);
      server_table->ApplyRowOpLog(row_id, column_ids, updates, num_updates);
    }

    updates = oplog_reader.Next(&table_id, &row_id, &column_ids,
      &num_updates, &started_new_table);
    if(updates == 0)
      break;
    if(started_new_table){
      auto table_iter = tables_.find(table_id);
      server_table = &(table_iter->second);
    }
  }
  VLOG(0) << "Read and Apply Update Done";
}

int32_t Server::GetMinClock() {
  return client_clocks_.get_min_clock();
}

int32_t Server::GetBgVersion(int32_t bg_thread_id) {
  return bg_version_map_[bg_thread_id];
}

void Server::CreateSendServerPushRowMsgs(PushMsgSendFunc PushMsgSend) {
  int32_t client_id = 0;
  boost::unordered_map<int32_t, RecordBuff> buffs;
  boost::unordered_map<int32_t, ServerPushRowMsg*> msg_map;

  // Create a message for each bg thread
  for (client_id = 0; client_id < GlobalContext::get_num_clients();
       ++client_id) {
    int32_t head_bg_id = GlobalContext::get_head_bg_id(client_id);
    for (int32_t i = 0; i < GlobalContext::get_num_bg_threads(); ++i) {
      int32_t bg_id = head_bg_id + i;
      ServerPushRowMsg *msg = new ServerPushRowMsg(push_row_msg_data_size_);
      msg_map[bg_id] = msg;
      buffs.insert(std::make_pair(bg_id,
                   RecordBuff(msg->get_data(), push_row_msg_data_size_)));
      //VLOG(0) << "Created msg for bg " << bg_id;
    }
  }

  int32_t num_tables_left = GlobalContext::get_num_tables();
  for (auto table_iter = tables_.begin(); table_iter != tables_.end();
       table_iter++) {
    int32_t table_id = table_iter->first;
    VLOG(0) << "Serializing table " << table_id;
    ServerTable &server_table = table_iter->second;
    for (client_id = 0; client_id < GlobalContext::get_num_clients();
         ++client_id) {
      int32_t head_bg_id = GlobalContext::get_head_bg_id(client_id);

      // Set table id
      for (int32_t i = 0; i < GlobalContext::get_num_bg_threads(); ++i) {
        int32_t bg_id = head_bg_id + i;
        RecordBuff &record_buff = buffs[bg_id];
        int32_t *table_id_ptr = record_buff.GetMemPtrInt32();
        if (table_id_ptr == 0) {
          VLOG(0) << "Not enough space for table id, send out to " << bg_id;
          PushMsgSend(bg_id, msg_map[bg_id], false);
          memset((msg_map[bg_id])->get_data(), 0, push_row_msg_data_size_);
          record_buff.ResetOffset();
          table_id_ptr = record_buff.GetMemPtrInt32();
        }
        *table_id_ptr = table_id;
      }
    }
    //ServerTable packs the data.
    server_table.InitAppendTableToBuffs();
    int32_t failed_bg_id;
    int32_t failed_client_id;
    bool pack_suc = server_table.AppendTableToBuffs(0, &buffs, &failed_bg_id,
                                                    &failed_client_id, false);
    while (!pack_suc) {
      VLOG(0) << "Not enough space for appending row, send out to "
	      << failed_bg_id;

      RecordBuff &record_buff = buffs[failed_bg_id];
      int32_t *buff_end_ptr = record_buff.GetMemPtrInt32();
      if (buff_end_ptr != 0)
        *buff_end_ptr = GlobalContext::get_serialized_table_end();

      PushMsgSend(failed_bg_id, msg_map[failed_bg_id], false);
      //VLOG(0) << "PushMsgSend done()";
      memset((msg_map[failed_bg_id])->get_data(), 0, push_row_msg_data_size_);
      //VLOG(0) << "Reset memory";
      record_buff.ResetOffset();
      //VLOG(0) << "Resume appending ";
      int32_t *table_id_ptr = record_buff.GetMemPtrInt32();
      *table_id_ptr = table_id;
      pack_suc = server_table.AppendTableToBuffs(failed_client_id, &buffs,
                                                 &failed_bg_id,
                                                 &failed_client_id, true);
    }

    --num_tables_left;
    //VLOG(0) << "num_tables_left = " << num_tables_left;
    if (num_tables_left > 0) {
      for (client_id = 0; client_id < GlobalContext::get_num_clients();
           ++client_id) {
        int32_t head_bg_id = GlobalContext::get_head_bg_id(client_id);
        // Set separator between tables
        for (int32_t i = 0; i < GlobalContext::get_num_bg_threads(); ++i) {
          int32_t bg_id = head_bg_id + i;
          RecordBuff &record_buff = buffs[bg_id];
          int32_t *table_sep_ptr = record_buff.GetMemPtrInt32();
          if (table_sep_ptr == 0) {
            VLOG(0) << "Not enough space for table separator, send out to "
              << bg_id;
            PushMsgSend(bg_id, msg_map[bg_id], false);
            memset((msg_map[bg_id])->get_data(), 0, push_row_msg_data_size_);
            record_buff.ResetOffset();
          } else {
            *table_sep_ptr = GlobalContext::get_serialized_table_separator();
          }
        }
      }
    } else
      break;
  }

  for (client_id = 0; client_id < GlobalContext::get_num_clients();
       ++client_id) {
    //VLOG(0) << "Send out to client " << client_id;
    int32_t head_bg_id = GlobalContext::get_head_bg_id(client_id);
    // Set separator between tables
    for (int32_t i = 0; i < GlobalContext::get_num_bg_threads(); ++i) {
      int32_t bg_id = head_bg_id + i;
      RecordBuff &record_buff = buffs[bg_id];
      int32_t *table_end_ptr = record_buff.GetMemPtrInt32();
      if (table_end_ptr == 0) {
        VLOG(0) << "Not enough space for table end, send out to "
                << bg_id;
        PushMsgSend(bg_id, msg_map[bg_id], true);
        continue;
      }
      *table_end_ptr = GlobalContext::get_serialized_table_end();
      msg_map[bg_id]->get_avai_size() = buffs[bg_id].GetMemUsedSize();
      //VLOG(0) << "End! Send msg out to " << bg_id;
      PushMsgSend(bg_id, msg_map[bg_id], true);
      delete msg_map[bg_id];
    }
  }
}

}  // namespace petuum
