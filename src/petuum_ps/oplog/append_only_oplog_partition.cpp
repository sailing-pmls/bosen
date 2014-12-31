#include <petuum_ps/oplog/append_only_oplog_partition.hpp>
#include <petuum_ps_common/oplog/batch_inc_append_only_buffer.hpp>
#include <petuum_ps_common/oplog/inc_append_only_buffer.hpp>
#include <petuum_ps_common/oplog/dense_append_only_buffer.hpp>
#include <petuum_ps_common/util/stats.hpp>
#include <petuum_ps/thread/context.hpp>
namespace petuum {

AppendOnlyOpLogPartition::AppendOnlyOpLogPartition(
    size_t buffer_capacity, const AbstractRow *sample_row,
    AppendOnlyOpLogType append_only_oplog_type,
    size_t dense_row_capacity,
    size_t per_thread_pool_size):
    buffer_capacity_(buffer_capacity),
    update_size_(sample_row->get_update_size()),
    sample_row_(sample_row),
    append_only_oplog_type_(append_only_oplog_type),
    dense_row_capacity_(dense_row_capacity),
    per_thread_pool_size_(per_thread_pool_size),
    // make the queue size large enough so the app thread only blocks on
    // buffer pool
    shared_queue_(per_thread_pool_size*GlobalContext::get_num_table_threads()),
    oplog_buffer_mgr_(GlobalContext::get_num_table_threads(),
                      GlobalContext::get_head_table_thread_id()) { }

AppendOnlyOpLogPartition::~AppendOnlyOpLogPartition() { }

void AppendOnlyOpLogPartition::RegisterThread() {
  oplog_buffer_mgr_.CreateBufferPool(
      ThreadContext::get_id(), per_thread_pool_size_,
      append_only_oplog_type_, buffer_capacity_, update_size_,
      dense_row_capacity_);

  if (append_only_buff_.get() == 0)
    append_only_buff_.reset(oplog_buffer_mgr_.GetBuff(ThreadContext::get_id()));
}

void AppendOnlyOpLogPartition::DeregisterThread() {
  append_only_buff_.release();
}

void AppendOnlyOpLogPartition::FlushOpLog() {
  STATS_APP_ACCUM_APPEND_ONLY_FLUSH_OPLOG_BEGIN();
  shared_queue_.Push(append_only_buff_.get());

  append_only_buff_.release();
  append_only_buff_.reset(oplog_buffer_mgr_.GetBuff(ThreadContext::get_id()));
  STATS_APP_ACCUM_APPEND_ONLY_FLUSH_OPLOG_END();
}

int32_t AppendOnlyOpLogPartition::Inc(int32_t row_id, int32_t column_id,
                                      const void *delta) {
  if(!append_only_buff_->Inc(row_id, column_id, delta)) {
    FlushOpLog();
    append_only_buff_->Inc(row_id, column_id, delta);
    return 1;
  }
  return 0;
}

int32_t AppendOnlyOpLogPartition::BatchInc(int32_t row_id, const int32_t *column_ids,
                                        const void *deltas, int32_t num_updates) {
  if(!append_only_buff_->BatchInc(row_id, column_ids, deltas, num_updates)) {
    FlushOpLog();
    append_only_buff_->BatchInc(row_id, column_ids, deltas, num_updates);
    return 1;
  }
  return 0;
}

int32_t AppendOnlyOpLogPartition::DenseBatchInc(
    int32_t row_id, const void *updates,
    int32_t index_st, int32_t num_updates) {
  if(!append_only_buff_->DenseBatchInc
     (row_id, updates, index_st, num_updates)) {
    FlushOpLog();
    append_only_buff_->DenseBatchInc(row_id, updates, index_st, num_updates);
    return 1;
  }
  return 0;
}

bool AppendOnlyOpLogPartition::FindOpLog(
    int32_t row_id, OpLogAccessor *oplog_accessor) {
  LOG(FATAL) << "Operation not supported!";
  return false;
}

bool AppendOnlyOpLogPartition::FindInsertOpLog(
    int32_t row_id, OpLogAccessor *oplog_accessor) {
  LOG(FATAL) << "Operation not supported!";
  return false;
}

bool AppendOnlyOpLogPartition::FindAndLock(
    int32_t row_id, OpLogAccessor *oplog_accessor) {
  LOG(FATAL) << "Operation not supported!";
  return false;
}

AbstractRowOpLog *AppendOnlyOpLogPartition::FindOpLog(int32_t row_id) {
  LOG(FATAL) << "Operation not supported!";
  return 0;
}

AbstractRowOpLog *AppendOnlyOpLogPartition::FindInsertOpLog(int32_t row_id) {
  LOG(FATAL) << "Operation not supported!";
  return 0;
}

bool AppendOnlyOpLogPartition::GetEraseOpLog(
    int32_t row_id, AbstractRowOpLog **row_oplog_ptr) {
  LOG(FATAL) << "Operation not supported!";
  return false;
}

bool AppendOnlyOpLogPartition::GetEraseOpLogIf(
    int32_t row_id, GetOpLogTestFunc test,
    void *test_args, AbstractRowOpLog **row_oplog_ptr) {
  LOG(FATAL) << "Operation not supported!";
  return false;
}

bool AppendOnlyOpLogPartition::GetInvalidateOpLogMeta(
    int32_t row_id, RowOpLogMeta *row_oplog_meta) {
  LOG(FATAL) << "Operation not supported!";
  return false;
}

AbstractAppendOnlyBuffer *AppendOnlyOpLogPartition::GetAppendOnlyBuffer(
    int32_t comm_channel_idx __attribute__ ((unused)) ) {
  AbstractAppendOnlyBuffer *buff_ptr = 0;

  if(shared_queue_.Pop(&buff_ptr))
    return buff_ptr;

  return 0;
}

void AppendOnlyOpLogPartition::PutBackBuffer(
    int32_t comm_channel_idx __attribute__ ((unused)),
    AbstractAppendOnlyBuffer *buff) {
  buff->ResetSize();

  int32_t thread_id = buff->get_thread_id();

  oplog_buffer_mgr_.PutBuff(thread_id, buff);
}

}
