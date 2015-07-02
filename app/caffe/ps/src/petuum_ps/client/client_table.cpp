#include <glog/logging.h>
#include <petuum_ps/client/client_table.hpp>
#include <petuum_ps_common/util/class_register.hpp>
#include <petuum_ps_common/util/stats.hpp>
#include <petuum_ps_common/client/client_row.hpp>
#include <petuum_ps_common/storage/bounded_dense_process_storage.hpp>
#include <petuum_ps_common/storage/bounded_sparse_process_storage.hpp>
#include <petuum_ps_common/util/class_register.hpp>

#include <petuum_ps/client/ssp_client_row.hpp>
#include <petuum_ps/consistency/ssp_consistency_controller.hpp>
#include <petuum_ps/consistency/ssp_push_consistency_controller.hpp>
#include <petuum_ps/consistency/ssp_push_append_only_consistency_controller.hpp>
#include <petuum_ps/consistency/ssp_aggr_consistency_controller.hpp>
#include <petuum_ps/consistency/ssp_aggr_value_consistency_controller.hpp>
#include <petuum_ps/thread/context.hpp>

#include <petuum_ps/oplog/sparse_oplog.hpp>
#include <petuum_ps/oplog/dense_oplog.hpp>
#include <petuum_ps/oplog/append_only_oplog.hpp>

#include <cmath>

namespace petuum {

ClientTable::ClientTable(int32_t table_id, const ClientTableConfig &config):
    AbstractClientTable(),
    table_id_(table_id), row_type_(config.table_info.row_type),
    sample_row_(ClassRegistry<AbstractRow>::GetRegistry().CreateObject(
        row_type_)),
    oplog_index_(config.oplog_capacity),
    staleness_(config.table_info.table_staleness),
    oplog_dense_serialized_(config.table_info.oplog_dense_serialized),
    client_table_config_(config),
    oplog_type_(config.oplog_type),
    bg_apply_append_oplog_freq_(config.bg_apply_append_oplog_freq),
    row_oplog_type_(config.table_info.row_oplog_type),
    dense_row_oplog_capacity_(config.table_info.dense_row_oplog_capacity),
    append_only_oplog_type_(config.append_only_oplog_type),
    row_capacity_(config.table_info.row_capacity),
    no_oplog_replay_(config.no_oplog_replay) {
  switch (config.process_storage_type) {
    case BoundedDense:
      {
        BoundedDenseProcessStorage::CreateClientRowFunc StorageCreateClientRow;
        if (GlobalContext::get_consistency_model() == SSP) {
          StorageCreateClientRow = std::bind(&ClientTable::CreateSSPClientRow, this,
                                             std::placeholders::_1);
        } else if (GlobalContext::get_consistency_model() == SSPPush
                   || GlobalContext::get_consistency_model() == SSPAggr) {
          StorageCreateClientRow = std::bind(&ClientTable::CreateClientRow, this,
                                             std::placeholders::_1);
        } else {
          LOG(FATAL) << "Unknown consistency model " << GlobalContext::get_consistency_model();
        }

        process_storage_ = static_cast<AbstractProcessStorage*>(
            new BoundedDenseProcessStorage(
                config.process_cache_capacity, StorageCreateClientRow, 0));
      }
      break;
    case BoundedSparse:
      {
        process_storage_ = static_cast<AbstractProcessStorage*>(
            new BoundedSparseProcessStorage(
            config.process_cache_capacity,
            GlobalContext::GetLockPoolSize(config.process_cache_capacity)));
      }
      break;
    default:
      LOG(FATAL) << "Unknown process storage type " << config.process_storage_type;
  }

  switch (config.oplog_type) {
    case Sparse:
      oplog_ = new SparseOpLog(config.oplog_capacity, sample_row_,
                               dense_row_oplog_capacity_, row_oplog_type_,
                               config.table_info.version_maintain);
      break;
    case AppendOnly:
      oplog_ = new AppendOnlyOpLog(
          config.append_only_buff_capacity,
          sample_row_,
          config.append_only_oplog_type,
          dense_row_oplog_capacity_,
          config.per_thread_append_only_buff_pool_size);
      break;
    case Dense:
      oplog_ = new DenseOpLog(
          config.oplog_capacity,
          sample_row_,
          dense_row_oplog_capacity_,
          row_oplog_type_,
          config.table_info.version_maintain);
      break;
    default:
      LOG(FATAL) << "Unknown oplog type = " << config.oplog_type;
  }

  switch (GlobalContext::get_consistency_model()) {
    case SSP:
      {
        consistency_controller_
            = new SSPConsistencyController(
                config.table_info,
                table_id, *process_storage_, *oplog_, sample_row_, thread_cache_,
                oplog_index_, row_oplog_type_);
      }
      break;
    case SSPPush:
      {
        if (config.oplog_type == Sparse || config.oplog_type == Dense) {
        consistency_controller_
            = new SSPPushConsistencyController(
                config.table_info,
                table_id, *process_storage_, *oplog_, sample_row_, thread_cache_,
                oplog_index_, row_oplog_type_);
        } else if (config.oplog_type == AppendOnly) {
          consistency_controller_
            = new SSPPushAppendOnlyConsistencyController(
                config.table_info, table_id, *process_storage_, *oplog_,
                sample_row_, thread_cache_, oplog_index_, row_oplog_type_);
        } else {
          LOG(FATAL) << "Unknown oplog type = " << config.oplog_type;
        }
      }
      break;
    case SSPAggr:
      {
        UpdateSortPolicy update_sort_policy
            = GlobalContext::get_update_sort_policy();
#ifdef PETUUM_COMP_IMPORTANCE
        if (update_sort_policy == RelativeMagnitude
            || update_sort_policy == FIFO_N_ReMag
            || update_sort_policy == Random) {
#else
        if (update_sort_policy == RelativeMagnitude
            || update_sort_policy == FIFO_N_ReMag) {
#endif
          consistency_controller_
              = new SSPAggrValueConsistencyController(
                  config.table_info, table_id, *process_storage_,
                  *oplog_, sample_row_, thread_cache_, oplog_index_,
                  row_oplog_type_);
        } else {
          consistency_controller_
              = new SSPAggrConsistencyController(
                  config.table_info, table_id, *process_storage_,
                  *oplog_, sample_row_, thread_cache_, oplog_index_,
                  row_oplog_type_);
        }
      }
      break;
    default:
      LOG(FATAL) << "Not yet support consistency model "
                 << GlobalContext::get_consistency_model();
  }
}

ClientTable::~ClientTable() {
  delete consistency_controller_;
  delete sample_row_;
  delete oplog_;
  delete process_storage_;
}

void ClientTable::RegisterThread() {
  if (thread_cache_.get() == 0)
    thread_cache_.reset(new ThreadTable(
        sample_row_, client_table_config_.table_info.row_oplog_type,
        client_table_config_.table_info.row_capacity));

  oplog_->RegisterThread();
}

void ClientTable::DeregisterThread() {
  thread_cache_.reset(0);
  oplog_->DeregisterThread();
}

void ClientTable::GetAsyncForced(int32_t row_id) {
  consistency_controller_->GetAsyncForced(row_id);
}

void ClientTable::GetAsync(int32_t row_id) {
  consistency_controller_->GetAsync(row_id);
}

void ClientTable::WaitPendingAsyncGet() {
  consistency_controller_->WaitPendingAsnycGet();
}

void ClientTable::ThreadGet(int32_t row_id, ThreadRowAccessor *row_accessor) {
  consistency_controller_->ThreadGet(row_id, row_accessor);
}

void ClientTable::ThreadInc(int32_t row_id, int32_t column_id,
                            const void *update) {
  consistency_controller_->ThreadInc(row_id, column_id, update);
}
void ClientTable::ThreadBatchInc(int32_t row_id, const int32_t* column_ids,
                                 const void* updates,
                                 int32_t num_updates) {
  consistency_controller_->ThreadBatchInc(row_id, column_ids, updates,
    num_updates);
}

void ClientTable::FlushThreadCache() {
  consistency_controller_->FlushThreadCache();
}

ClientRow *ClientTable::Get(int32_t row_id, RowAccessor *row_accessor,
                            int32_t clock) {
  return consistency_controller_->Get(row_id, row_accessor, clock);
}

void ClientTable::Inc(int32_t row_id, int32_t column_id, const void *update) {
  STATS_APP_SAMPLE_INC_BEGIN(table_id_);
  consistency_controller_->Inc(row_id, column_id, update);
  STATS_APP_SAMPLE_INC_END(table_id_);
}

void ClientTable::BatchInc(int32_t row_id, const int32_t* column_ids,
  const void* updates, int32_t num_updates) {
  STATS_APP_SAMPLE_BATCH_INC_BEGIN(table_id_);
  consistency_controller_->BatchInc(row_id, column_ids, updates,
                                    num_updates);
  STATS_APP_SAMPLE_BATCH_INC_END(table_id_);
}

void ClientTable::DenseBatchInc(
    int32_t row_id, const void *updates, int32_t index_st,
    int32_t num_updates) {
  STATS_APP_SAMPLE_BATCH_INC_BEGIN(table_id_);
  consistency_controller_->DenseBatchInc(row_id, updates, index_st,
                                         num_updates);
  STATS_APP_SAMPLE_BATCH_INC_END(table_id_);
}

void ClientTable::ThreadDenseBatchInc(int32_t row_id, const void *updates,
                                      int32_t index_st,
                                      int32_t num_updates) {
  consistency_controller_->ThreadDenseBatchInc(row_id, updates, index_st,
                                               num_updates);
}


void ClientTable::Clock() {
  STATS_APP_SAMPLE_CLOCK_BEGIN(table_id_);
  consistency_controller_->Clock();
  STATS_APP_SAMPLE_CLOCK_END(table_id_);
}

cuckoohash_map<int32_t, bool> *ClientTable::GetAndResetOpLogIndex(
    int32_t partition_num) {
  return oplog_index_.ResetPartition(partition_num);
}

size_t ClientTable::GetNumRowOpLogs(int32_t partition_num) {
  return oplog_index_.GetNumRowOpLogs(partition_num);
}

ClientRow *ClientTable::CreateClientRow(int32_t clock) {
  AbstractRow *row_data = ClassRegistry<AbstractRow>::GetRegistry().CreateObject(row_type_);
  row_data->Init(row_capacity_);
  return new ClientRow(clock, row_data, false);
}

ClientRow *ClientTable::CreateSSPClientRow(int32_t clock) {
  AbstractRow *row_data = ClassRegistry<AbstractRow>::GetRegistry().CreateObject(row_type_);
  row_data->Init(row_capacity_);
  return static_cast<ClientRow*>(new SSPClientRow(clock, row_data, false));
}

}  // namespace petuum
