#include <petuum_ps/thread/ssp_aggr_bg_worker.hpp>
#include <petuum_ps/thread/trans_time_estimate.hpp>
#include <petuum_ps_common/util/stats.hpp>

namespace petuum {

void SSPAggrBgWorker::SetWaitMsg() {
  WaitMsg_ = WaitMsgTimeOut;
}

void SSPAggrBgWorker::PrepareBeforeInfiniteLoop() {
  msg_send_timer_.restart();
}

void SSPAggrBgWorker::FinalizeTableStats() {
  for (const auto &table_pair : (*tables_)) {
    min_table_staleness_
        = std::min(min_table_staleness_,
                   table_pair.second->get_staleness());
  }
}

long SSPAggrBgWorker::ResetBgIdleMilli() {
  return GlobalContext::get_bg_idle_milli();
}

void SSPAggrBgWorker::ReadTableOpLogsIntoOpLogMeta(int32_t table_id,
                                                   ClientTable *table) {
  // Get OpLog index
  cuckoohash_map<int32_t, bool> *new_table_oplog_index_ptr
      = table->GetAndResetOpLogIndex(my_comm_channel_idx_);

  AbstractOpLog &table_oplog = table->get_oplog();
  TableOpLogMeta *table_oplog_meta = oplog_meta_.Get(table_id);

  if (table_oplog_meta == 0) {
    const AbstractRow *sample_row = table->get_sample_row();
    table_oplog_meta = oplog_meta_.AddTableOpLogMeta(table_id, sample_row);
  }

  for (auto oplog_index_iter = new_table_oplog_index_ptr->cbegin();
       !oplog_index_iter.is_end(); oplog_index_iter++) {
    int32_t row_id = oplog_index_iter->first;
    RowOpLogMeta row_oplog_meta;
    bool found = table_oplog.GetInvalidateOpLogMeta(row_id, &row_oplog_meta);
    if (!found || (row_oplog_meta.get_clock() == -1)) {
      continue;
    }

    table_oplog_meta->InsertMergeRowOpLogMeta(row_id, row_oplog_meta);
  }
  delete new_table_oplog_index_ptr;
}

size_t SSPAggrBgWorker::ReadTableOpLogMetaUpToClock(
    int32_t table_id, ClientTable *table, int32_t clock_to_push,
    TableOpLogMeta *table_oplog_meta,
    GetSerializedRowOpLogSizeFunc GetSerializedRowOpLogSize,
    BgOpLogPartition *bg_table_oplog) {

  if (clock_to_push < 0)
    return 0;

  size_t accum_table_oplog_bytes = 0;

  AbstractOpLog &table_oplog = table->get_oplog();

  int32_t row_id;
  row_id = table_oplog_meta->InitGetUptoClock(clock_to_push);
  while (row_id >= 0) {
    AbstractRowOpLog* row_oplog = 0;
    bool found = table_oplog.GetEraseOpLog(row_id, &row_oplog);

    if (found && row_oplog != 0) {
      size_t serialized_oplog_size = CountRowOpLogToSend(
          row_id, row_oplog, &table_num_bytes_by_server_,
          bg_table_oplog, GetSerializedRowOpLogSize);

      accum_table_oplog_bytes += serialized_oplog_size;
    }

    row_id = table_oplog_meta->GetAndClearNextUptoClock();
  }
  return accum_table_oplog_bytes;
}

size_t SSPAggrBgWorker::ReadTableOpLogMetaUpToClockNoReplay(
    int32_t table_id, ClientTable *table, int32_t clock_to_push,
    TableOpLogMeta *table_oplog_meta,
    RowOpLogSerializer *row_oplog_serializer) {

  if (clock_to_push < 0)
    return 0;

  size_t accum_table_oplog_bytes = 0;

  AbstractOpLog &table_oplog = table->get_oplog();

  int32_t row_id;
  row_id = table_oplog_meta->InitGetUptoClock(clock_to_push);
  while (row_id >= 0) {
    OpLogAccessor oplog_accessor;
    bool found = table_oplog.FindAndLock(row_id, &oplog_accessor);

    if (found) {
      AbstractRowOpLog* row_oplog = oplog_accessor.get_row_oplog();
      if (row_oplog != 0) {
        size_t serialized_oplog_size
            = row_oplog_serializer->AppendRowOpLog(row_id, row_oplog);
        row_oplog->Reset();

        MetaRowOpLog *meta_row_oplog = dynamic_cast<MetaRowOpLog*>(row_oplog);
        meta_row_oplog->InvalidateMeta();
        meta_row_oplog->ResetImportance();

        accum_table_oplog_bytes += serialized_oplog_size;
    }

    row_id = table_oplog_meta->GetAndClearNextUptoClock();
    }
  }
  return accum_table_oplog_bytes;
}
  size_t SSPAggrBgWorker::ReadTableOpLogMetaUpToCapacity(
      int32_t table_id, ClientTable *table, size_t bytes_accumulated,
    TableOpLogMeta *table_oplog_meta,
    GetSerializedRowOpLogSizeFunc GetSerializedRowOpLogSize,
    BgOpLogPartition *bg_table_oplog) {

  size_t accum_table_oplog_bytes = bytes_accumulated;

  AbstractOpLog &table_oplog = table->get_oplog();

  table_oplog_meta->Sort();

  int32_t row_id;
  row_id = table_oplog_meta->GetAndClearNextInOrder();

  while (row_id >= 0) {
    AbstractRowOpLog* row_oplog = 0;
    bool found = table_oplog.GetEraseOpLog(row_id, &row_oplog);

    if (found && row_oplog != 0) {
      size_t serialized_oplog_size = CountRowOpLogToSend(
          row_id, row_oplog, &table_num_bytes_by_server_,
          bg_table_oplog, GetSerializedRowOpLogSize);
      accum_table_oplog_bytes += serialized_oplog_size;

      if (accum_table_oplog_bytes
          >= GlobalContext::get_oplog_push_upper_bound_kb()*k1_Ki)
        break;
    }

    row_id = table_oplog_meta->GetAndClearNextInOrder();
  }
  return accum_table_oplog_bytes;
}

size_t SSPAggrBgWorker::ReadTableOpLogMetaUpToCapacityNoReplay(
    int32_t table_id, ClientTable *table, size_t bytes_accumulated,
    TableOpLogMeta *table_oplog_meta,
    RowOpLogSerializer *row_oplog_serializer) {

  size_t accum_table_oplog_bytes = bytes_accumulated;

  AbstractOpLog &table_oplog = table->get_oplog();

  table_oplog_meta->Sort();

  int32_t row_id;
  row_id = table_oplog_meta->GetAndClearNextInOrder();

  while (row_id >= 0) {
    OpLogAccessor oplog_accessor;
    bool found = table_oplog.FindAndLock(row_id, &oplog_accessor);

    if (found) {
      AbstractRowOpLog* row_oplog = oplog_accessor.get_row_oplog();
      if (row_oplog != 0) {
        size_t serialized_oplog_size
            = row_oplog_serializer->AppendRowOpLog(row_id, row_oplog);
        row_oplog->Reset();

        MetaRowOpLog *meta_row_oplog = dynamic_cast<MetaRowOpLog*>(row_oplog);
        meta_row_oplog->InvalidateMeta();
        meta_row_oplog->ResetImportance();

        accum_table_oplog_bytes += serialized_oplog_size;

        if (accum_table_oplog_bytes
            >= GlobalContext::get_oplog_push_upper_bound_kb()*k1_Ki)
          break;
      }
      row_id = table_oplog_meta->GetAndClearNextInOrder();
    }
  }
  return accum_table_oplog_bytes;
}

BgOpLogPartition* SSPAggrBgWorker::PrepareOpLogsNormal(
    int32_t table_id, ClientTable *table, int32_t clock_to_push) {

  GetSerializedRowOpLogSizeFunc GetSerializedRowOpLogSize;
  if (table->oplog_dense_serialized()) {
    GetSerializedRowOpLogSize = GetDenseSerializedRowOpLogSize;
  } else {
    GetSerializedRowOpLogSize = GetSparseSerializedRowOpLogSize;
  }

  ReadTableOpLogsIntoOpLogMeta(table_id, table);

  for (const auto &server_id : server_ids_) {
    table_num_bytes_by_server_[server_id] = 0;
  }

  size_t table_update_size
      = table->get_sample_row()->get_update_size();

  BgOpLogPartition *bg_table_oplog = new BgOpLogPartition(
      table_id, table_update_size, my_comm_channel_idx_);

  TableOpLogMeta *table_oplog_meta = oplog_meta_.Get(table_id);

  size_t accum_table_oplog_bytes = ReadTableOpLogMetaUpToClock(
      table_id, table, clock_to_push, table_oplog_meta,
      GetSerializedRowOpLogSize, bg_table_oplog);

  ReadTableOpLogMetaUpToCapacity(
      table_id, table, accum_table_oplog_bytes,
      table_oplog_meta, GetSerializedRowOpLogSize,
      bg_table_oplog);

  return bg_table_oplog;
}

BgOpLogPartition* SSPAggrBgWorker::PrepareOpLogsAppendOnly(
    int32_t table_id, ClientTable *table, int32_t clock_to_push) {
  LOG(FATAL) << "Not yet supported!";
  return 0;
}

void SSPAggrBgWorker::PrepareOpLogsNormalNoReplay(
    int32_t table_id, ClientTable *table, int32_t clock_to_push) {

  auto serializer_iter = row_oplog_serializer_map_.find(table_id);

  if (serializer_iter == row_oplog_serializer_map_.end()) {
    RowOpLogSerializer *row_oplog_serializer
        = new RowOpLogSerializer(table->oplog_dense_serialized(),
                                 my_comm_channel_idx_);
    row_oplog_serializer_map_.insert(std::make_pair(table_id, row_oplog_serializer));
    serializer_iter = row_oplog_serializer_map_.find(table_id);
  }
  RowOpLogSerializer *row_oplog_serializer = serializer_iter->second;

  ReadTableOpLogsIntoOpLogMeta(table_id, table);

  TableOpLogMeta *table_oplog_meta = oplog_meta_.Get(table_id);

  size_t accum_table_oplog_bytes = ReadTableOpLogMetaUpToClockNoReplay(
      table_id, table, clock_to_push, table_oplog_meta,
      row_oplog_serializer);

  ReadTableOpLogMetaUpToCapacityNoReplay(
      table_id, table, accum_table_oplog_bytes,
      table_oplog_meta, row_oplog_serializer);

  for (const auto &server_id : server_ids_) {
    table_num_bytes_by_server_[server_id] = 0;
  }

  row_oplog_serializer->GetServerTableSizeMap(&table_num_bytes_by_server_);
}

void SSPAggrBgWorker::PrepareOpLogsAppendOnlyNoReplay(
    int32_t table_id, ClientTable *table, int32_t clock_to_push) {
  LOG(FATAL) << "Not yet supported!";
}

BgOpLog *SSPAggrBgWorker::PrepareOpLogsToSend(int32_t clock_to_push) {
  BgOpLog *bg_oplog = new BgOpLog;

  for (const auto &table_pair : (*tables_)) {
    int32_t table_id = table_pair.first;

    if (table_pair.second->get_no_oplog_replay()) {
      if (table_pair.second->get_oplog_type() == Sparse ||
          table_pair.second->get_oplog_type() == Dense)
        PrepareOpLogsNormalNoReplay(table_id, table_pair.second, clock_to_push);
      else if (table_pair.second->get_oplog_type() == AppendOnly)
        PrepareOpLogsAppendOnlyNoReplay(table_id, table_pair.second, clock_to_push);
      else
        LOG(FATAL) << "Unknown oplog type = " << table_pair.second->get_oplog_type();
    } else {
      BgOpLogPartition *bg_table_oplog = 0;
      if (table_pair.second->get_oplog_type() == Sparse ||
          table_pair.second->get_oplog_type() == Dense)
        bg_table_oplog = PrepareOpLogsNormal(table_id, table_pair.second, clock_to_push);
      else if (table_pair.second->get_oplog_type() == AppendOnly)
        bg_table_oplog = PrepareOpLogsAppendOnly(table_id, table_pair.second, clock_to_push);
      else
        LOG(FATAL) << "Unknown oplog type = " << table_pair.second->get_oplog_type();
      bg_oplog->Add(table_id, bg_table_oplog);
    }

    FinalizeOpLogMsgStats(table_id, &table_num_bytes_by_server_,
                          &server_table_oplog_size_map_);
  }
  return bg_oplog;
}

long SSPAggrBgWorker::HandleClockMsg(bool clock_advanced) {

  if (!clock_advanced)
    return GlobalContext::get_bg_idle_milli();

  int32_t clock_to_push
      = client_clock_ - min_table_staleness_
      + GlobalContext::get_oplog_push_staleness_tolerance();

  if (clock_to_push > client_clock_)
    clock_to_push = client_clock_;

  if (clock_to_push >= 0)
    clock_has_pushed_ = clock_to_push;

  BgOpLog *bg_oplog = PrepareOpLogsToSend(clock_to_push);

  CreateOpLogMsgs(bg_oplog);
  size_t sent_size = SendOpLogMsgs(true);
  TrackBgOpLog(bg_oplog);

  oplog_send_milli_sec_
      = TransTimeEstimate::EstimateTransMillisec(sent_size);

  msg_send_timer_.restart();

  STATS_BG_ACCUM_IDLE_SEND_END();

  STATS_BG_ACCUM_IDLE_OPLOG_SENT_BYTES(sent_size);

  VLOG(0) << "BgIdle send bytes = " << sent_size
          << " send milli sec = " << oplog_send_milli_sec_
          << " bg_id = " << my_id_;

  return oplog_send_milli_sec_;
}

BgOpLog *SSPAggrBgWorker::PrepareBgIdleOpLogs() {
  BgOpLog *bg_oplog = new BgOpLog;

  for (const auto &table_pair : (*tables_)) {
    int32_t table_id = table_pair.first;

    if (table_pair.second->get_no_oplog_replay()) {
      if (table_pair.second->get_oplog_type() == Sparse ||
          table_pair.second->get_oplog_type() == Dense)
        PrepareBgIdleOpLogsNormalNoReplay(table_id, table_pair.second);
      else if (table_pair.second->get_oplog_type() == AppendOnly)
        PrepareBgIdleOpLogsAppendOnlyNoReplay(table_id, table_pair.second);
      else
        LOG(FATAL) << "Unknown oplog type = " << table_pair.second->get_oplog_type();
    } else {
      BgOpLogPartition *bg_table_oplog = 0;
      if (table_pair.second->get_oplog_type() == Sparse ||
          table_pair.second->get_oplog_type() == Dense)
        bg_table_oplog = PrepareBgIdleOpLogsNormal(table_id, table_pair.second);
      else if (table_pair.second->get_oplog_type() == AppendOnly)
        bg_table_oplog = PrepareBgIdleOpLogsAppendOnly(table_id, table_pair.second);
      else
        LOG(FATAL) << "Unknown oplog type = " << table_pair.second->get_oplog_type();
      bg_oplog->Add(table_id, bg_table_oplog);
    }

    FinalizeOpLogMsgStats(table_id, &table_num_bytes_by_server_,
                          &server_table_oplog_size_map_);
  }
  return bg_oplog;
}

BgOpLogPartition* SSPAggrBgWorker::PrepareBgIdleOpLogsNormal(int32_t table_id,
                                                             ClientTable *table) {

  GetSerializedRowOpLogSizeFunc GetSerializedRowOpLogSize;
    if (table->oplog_dense_serialized()) {
      GetSerializedRowOpLogSize = GetDenseSerializedRowOpLogSize;
    } else {
      GetSerializedRowOpLogSize = GetSparseSerializedRowOpLogSize;
    }

    ReadTableOpLogsIntoOpLogMeta(table_id, table);

    for (const auto &server_id : server_ids_) {
      table_num_bytes_by_server_[server_id] = 0;
    }

    size_t table_update_size
        = table->get_sample_row()->get_update_size();
    BgOpLogPartition *bg_table_oplog = new BgOpLogPartition(
        table_id, table_update_size, my_comm_channel_idx_);

    TableOpLogMeta *table_oplog_meta = oplog_meta_.Get(table_id);

    ReadTableOpLogMetaUpToCapacity(
        table_id, table, 0, table_oplog_meta, GetSerializedRowOpLogSize,
        bg_table_oplog);

    return bg_table_oplog;
}

void SSPAggrBgWorker::PrepareBgIdleOpLogsNormalNoReplay(int32_t table_id,
                                                        ClientTable *table) {
  auto serializer_iter = row_oplog_serializer_map_.find(table_id);

  if (serializer_iter == row_oplog_serializer_map_.end()) {
    RowOpLogSerializer *row_oplog_serializer
        = new RowOpLogSerializer(table->oplog_dense_serialized(),
                                 my_comm_channel_idx_);
    row_oplog_serializer_map_.insert(std::make_pair(table_id, row_oplog_serializer));
    serializer_iter = row_oplog_serializer_map_.find(table_id);
  }
  RowOpLogSerializer *row_oplog_serializer = serializer_iter->second;

  ReadTableOpLogsIntoOpLogMeta(table_id, table);
  TableOpLogMeta *table_oplog_meta = oplog_meta_.Get(table_id);

  ReadTableOpLogMetaUpToCapacityNoReplay(
      table_id, table, 0,
      table_oplog_meta, row_oplog_serializer);

  for (const auto &server_id : server_ids_) {
    table_num_bytes_by_server_[server_id] = 0;
  }

  row_oplog_serializer->GetServerTableSizeMap(&table_num_bytes_by_server_);
}

BgOpLogPartition *SSPAggrBgWorker::PrepareBgIdleOpLogsAppendOnly(int32_t table_id,
                                                                 ClientTable *table) {

  LOG(FATAL) << "Operation not supported!";
  return 0;
}

void SSPAggrBgWorker::PrepareBgIdleOpLogsAppendOnlyNoReplay(int32_t table_id,
                                                            ClientTable *table) {

  LOG(FATAL) << "Operation not supported!";
}

long SSPAggrBgWorker::BgIdleWork() {
  STATS_BG_IDLE_INVOKE_INC_ONE();

  bool found_oplog = false;

  // check if last msg has been sent out
  if (oplog_send_milli_sec_ > 1) {
    double send_elapsed_milli = msg_send_timer_.elapsed() * kOneThousand;
    if (oplog_send_milli_sec_ > send_elapsed_milli + 1)
      return (oplog_send_milli_sec_ - send_elapsed_milli);
  }

  if (oplog_meta_.OpLogMetaExists())
    found_oplog = true;
  else {
    for (const auto &table_pair : (*tables_)) {
      if (table_pair.second->GetNumRowOpLogs(my_comm_channel_idx_) > 0) {
        found_oplog = true;
        break;
      }
    }
  }

  if (!found_oplog) {
    oplog_send_milli_sec_ = 0;
    return GlobalContext::get_bg_idle_milli();
  }

  STATS_BG_IDLE_SEND_INC_ONE();
  STATS_BG_ACCUM_IDLE_SEND_BEGIN();

  BgOpLog *bg_oplog = PrepareBgIdleOpLogs();

  CreateOpLogMsgs(bg_oplog);
  size_t sent_size = SendOpLogMsgs(false);
  TrackBgOpLog(bg_oplog);

  oplog_send_milli_sec_
      = TransTimeEstimate::EstimateTransMillisec(sent_size);

  msg_send_timer_.restart();

  STATS_BG_ACCUM_IDLE_SEND_END();

  STATS_BG_ACCUM_IDLE_OPLOG_SENT_BYTES(sent_size);

  VLOG(0) << "BgIdle send bytes = " << sent_size
            << " send milli sec = " << oplog_send_milli_sec_
            << " bg_id = " << ThreadContext::get_id();
  return oplog_send_milli_sec_;
}

}
