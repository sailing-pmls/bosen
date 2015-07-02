#include <petuum_ps/consistency/ssp_push_append_only_consistency_controller.hpp>
#include <petuum_ps/thread/context.hpp>
#include <petuum_ps/thread/bg_workers.hpp>
#include <petuum_ps_common/util/stats.hpp>
#include <glog/logging.h>

namespace petuum {

SSPPushAppendOnlyConsistencyController::SSPPushAppendOnlyConsistencyController(
  const TableInfo& info,
  int32_t table_id,
  AbstractProcessStorage& process_storage,
  AbstractOpLog& oplog,
  const AbstractRow* sample_row,
  boost::thread_specific_ptr<ThreadTable> &thread_cache,
  TableOpLogIndex &oplog_index,
  int32_t row_oplog_type) :
  SSPPushConsistencyController(
      info, table_id, process_storage, oplog,
      sample_row, thread_cache, oplog_index, row_oplog_type) { }

void SSPPushAppendOnlyConsistencyController::Inc(
    int32_t row_id, int32_t column_id, const void* delta) {
  int32_t channel_idx = oplog_.Inc(row_id, column_id, delta);
  if (channel_idx >= 0) {
    BgWorkers::SignalHandleAppendOnlyBuffer(table_id_, channel_idx);
  }
}

void SSPPushAppendOnlyConsistencyController::BatchInc(int32_t row_id,
  const int32_t* column_ids, const void* updates, int32_t num_updates) {

  STATS_APP_SAMPLE_BATCH_INC_OPLOG_BEGIN();

  int32_t channel_idx = oplog_.BatchInc(
      row_id, column_ids, updates, num_updates);

  if (channel_idx >= 0) {
    BgWorkers::SignalHandleAppendOnlyBuffer(table_id_, channel_idx);
  }

  STATS_APP_SAMPLE_BATCH_INC_OPLOG_END();
}

void SSPPushAppendOnlyConsistencyController::DenseBatchInc(
    int32_t row_id, const void *updates,
    int32_t index_st, int32_t num_updates) {
  STATS_APP_SAMPLE_BATCH_INC_OPLOG_BEGIN();

  int32_t channel_idx = oplog_.DenseBatchInc(
      row_id, updates, index_st, num_updates);

  if (channel_idx >= 0) {
    BgWorkers::SignalHandleAppendOnlyBuffer(table_id_, channel_idx);
  }

  STATS_APP_SAMPLE_BATCH_INC_OPLOG_END();
}

void SSPPushAppendOnlyConsistencyController::ThreadInc(
    int32_t row_id, int32_t column_id, const void* delta) {
  int32_t channel_idx = oplog_.Inc(row_id, column_id, delta);
  if (channel_idx >= 0) {
    BgWorkers::SignalHandleAppendOnlyBuffer(table_id_, channel_idx);
  }
}

void SSPPushAppendOnlyConsistencyController::ThreadBatchInc(int32_t row_id,
  const int32_t* column_ids, const void* updates, int32_t num_updates) {
  int32_t channel_idx = oplog_.BatchInc(
      row_id, column_ids, updates, num_updates);

  if (channel_idx >= 0) {
    BgWorkers::SignalHandleAppendOnlyBuffer(table_id_, channel_idx);
  }
}

void SSPPushAppendOnlyConsistencyController::ThreadDenseBatchInc(
    int32_t row_id, const void *updates, int32_t index_st,
    int32_t num_updates) {
  int32_t channel_idx = oplog_.DenseBatchInc(
      row_id, updates, index_st, num_updates);

  if (channel_idx >= 0) {
    BgWorkers::SignalHandleAppendOnlyBuffer(table_id_, channel_idx);
  }
}

void SSPPushAppendOnlyConsistencyController::FlushThreadCache() {
  oplog_.FlushOpLog();
  for (int i = 0; i < GlobalContext::get_num_comm_channels_per_client(); ++i) {
    BgWorkers::SignalHandleAppendOnlyBuffer(table_id_, i);
  }
}

void SSPPushAppendOnlyConsistencyController::Clock() {
  FlushThreadCache();
}

}
