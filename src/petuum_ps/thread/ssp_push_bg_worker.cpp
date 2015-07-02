#include <petuum_ps/thread/ssp_push_bg_worker.hpp>
#include <petuum_ps_common/util/stats.hpp>
#include <petuum_ps/client/serialized_row_reader.hpp>
#include <petuum_ps/thread/ssp_push_row_request_oplog_mgr.hpp>


namespace petuum {
void SSPPushBgWorker::CreateRowRequestOpLogMgr() {
  row_request_oplog_mgr_ = new SSPPushRowRequestOpLogMgr(server_ids_);
}

void SSPPushBgWorker::HandleServerPushRow(int32_t sender_id, void *msg_mem) {
  ServerPushRowMsg server_push_row_msg(msg_mem);
  uint32_t version = server_push_row_msg.get_version();
  row_request_oplog_mgr_->ServerAcknowledgeVersion(sender_id, version);

  bool is_clock = server_push_row_msg.get_is_clock();

  // Need to apply the new rows before waking up the app threads
  ApplyServerPushedRow(version, server_push_row_msg.get_data(),
                       server_push_row_msg.get_avai_size());

  STATS_BG_ADD_PER_CLOCK_SERVER_PUSH_ROW_SIZE(
      server_push_row_msg.get_size());
  STATS_BG_ACCUM_PUSH_ROW_MSG_RECEIVED_INC_ONE();

  if (is_clock) {
    int32_t server_clock = server_push_row_msg.get_clock();

    int32_t new_clock = server_vector_clock_.TickUntil(sender_id, server_clock);

    if (new_clock) {
      int32_t new_system_clock = bg_server_clock_->Tick(my_id_);

      if (new_system_clock) {
        *system_clock_ += 1;
        std::unique_lock<std::mutex> lock(*system_clock_mtx_);
        system_clock_cv_->notify_all();
      }
    }
  }
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
  //int32_t row_type = 0;
  ClientTable *client_table = NULL;
  while (data != NULL) {
    if (curr_table_id != table_id) {
      auto table_iter = tables_->find(table_id);
      CHECK(table_iter != tables_->end()) << "Cannot find table " << table_id;
      client_table = table_iter->second;
      curr_table_id = table_id;
    }

    STATS_BG_SAMPLE_PROCESS_CACHE_INSERT_BEGIN();
    RowAccessor row_accessor;
    ClientRow *client_row = client_table->get_process_storage().Find(
        row_id, &row_accessor);

    if (client_row != 0) {
      UpdateExistingRow(table_id, row_id, client_row, client_table, data, row_size, version);
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
