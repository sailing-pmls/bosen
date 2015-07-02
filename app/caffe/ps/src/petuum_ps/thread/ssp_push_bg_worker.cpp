#include <petuum_ps/thread/ssp_push_bg_worker.hpp>
#include <petuum_ps_common/util/stats.hpp>
#include <petuum_ps/client/serialized_row_reader.hpp>
#include <petuum_ps/thread/ssp_push_row_request_oplog_mgr.hpp>


namespace petuum {
void SSPPushBgWorker::CreateRowRequestOpLogMgr() {
  row_request_oplog_mgr_ = new SSPPushRowRequestOpLogMgr(server_ids_);
}

void SSPPushBgWorker::HandleServerPushRow(int32_t sender_id, ServerPushRowMsg &server_push_row_msg) {
  uint32_t version = server_push_row_msg.get_version();
  row_request_oplog_mgr_->ServerAcknowledgeVersion(sender_id, version);

  bool is_clock = server_push_row_msg.get_is_clock();

  // Need to apply the new rows before waking up the app threads
  ApplyServerPushedRow(version, server_push_row_msg.get_data(),
                       server_push_row_msg.get_avai_size());

  STATS_BG_ADD_PER_CLOCK_SERVER_PUSH_ROW_SIZE(
      server_push_row_msg.get_size());
  STATS_BG_ACCUM_PUSH_ROW_MSG_RECEIVED_INC_ONE();

  //LOG(INFO) << __func__;
  //if (!is_clock)
  //LOG(INFO) << "handle_server_push_row, is_clock = " << is_clock
  //        << " from " << sender_id
  //        << " size = " << server_push_row_msg.get_size()
  //        << " " << my_id_;

  if (is_clock) {
    int32_t server_clock = server_push_row_msg.get_clock();

    int32_t new_clock = server_vector_clock_.TickUntil(sender_id, server_clock);

    //LOG(INFO) << "handle_server_push_row, is_clock = " << is_clock
    //        << " clock = " << server_clock
    //        << " new clock = " << new_clock
    //        << " from " << sender_id
    //        << " size = " << server_push_row_msg.get_size()
    //        << " " << my_id_;

    if (new_clock) {
      int32_t new_system_clock = bg_server_clock_->TickUntil(
          my_id_, new_clock);
      //LOG(INFO) << "new_system_clock = " << new_system_clock;

      if (new_system_clock) {
        *system_clock_ = new_system_clock;
        std::unique_lock<std::mutex> lock(*system_clock_mtx_);
        system_clock_cv_->notify_all();
      }
    }
  }

  uint64_t seq = server_push_row_msg.get_seq_num();

  BgServerPushRowAckMsg ack_msg;
  ack_msg.get_ack_num() = seq;

  size_t sent_size = (comm_bus_->*(comm_bus_->SendAny_))(
     sender_id, ack_msg.get_mem(), ack_msg.get_size());
  CHECK_EQ(sent_size, ack_msg.get_size());

  //LOG(INFO) << "ack " << sender_id << " " << seq;
}

void SSPPushBgWorker::ApplyServerPushedRow(uint32_t version, void *mem,
                                           size_t mem_size) {
  STATS_BG_ACCUM_SERVER_PUSH_ROW_APPLY_BEGIN();

  SerializedRowReader row_reader(mem, mem_size);
  bool not_empty = row_reader.Restart();
  // no row to read
  if (!not_empty) return;

  int32_t table_id = 0;
  int32_t row_id = 0;
  size_t row_size = 0;
  const void *data = row_reader.Next(&table_id, &row_id, &row_size);

  int32_t curr_table_id = -1;
  bool curr_table_version_maintain = false;
  ClientTable *client_table = NULL;
  uint64_t row_version = 0;
  while (data != NULL) {
    STATS_BG_ACCUM_TABLE_ROW_RECVED(table_id, row_id, 1);
    if (curr_table_id != table_id) {
      auto table_iter = tables_->find(table_id);
      CHECK(table_iter != tables_->end()) << "Cannot find table " << table_id;
      client_table = table_iter->second;
      curr_table_id = table_id;
      curr_table_version_maintain = table_iter->second->get_version_maintain();
      if (table_id == 2) {
        CHECK(!curr_table_version_maintain) << "current table not version maintain";
      }
    }

    if (curr_table_version_maintain) {
      row_version = ExtractRowVersion(data, &row_size);
    }

    STATS_BG_SAMPLE_PROCESS_CACHE_INSERT_BEGIN();
    RowAccessor row_accessor;
    ClientRow *client_row = client_table->get_process_storage().Find(
        row_id, &row_accessor);

    if (client_row != 0) {
      //LOG(INFO) << "update " << row_id;
      UpdateExistingRow(table_id, row_id, client_row, client_table, data,
                        row_size, version, curr_table_version_maintain,
                        row_version);
    }

    STATS_BG_SAMPLE_PROCESS_CACHE_INSERT_END();

    data = row_reader.Next(&table_id, &row_id, &row_size);
  }
  STATS_BG_ACCUM_SERVER_PUSH_ROW_APPLY_END();
}

ClientRow *SSPPushBgWorker::CreateClientRow(int32_t clock,
                                            AbstractRow *row_data) {
  return new ClientRow(clock, row_data, true);
}

}
