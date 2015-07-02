//author: jinliang

#pragma once

#include <petuum_ps/thread/ssp_push_bg_worker.hpp>
#include <petuum_ps/thread/oplog_meta.hpp>
#include <petuum_ps/thread/bg_oplog_partition.hpp>
#include <petuum_ps_common/util/high_resolution_timer.hpp>

namespace petuum {

class SSPAggrBgWorker : public SSPPushBgWorker {
public:
  SSPAggrBgWorker(int32_t id, int32_t comm_channel_idx,
                  std::map<int32_t, ClientTable*> *tables,
                  pthread_barrier_t *init_barrier,
                  pthread_barrier_t *create_table_barrier,
                  std::atomic_int_fast32_t *system_clock,
                  std::mutex *system_clock_mtx,
                  std::condition_variable *system_clock_cv,
                  VectorClockMT *bg_server_clock):
      SSPPushBgWorker(id, comm_channel_idx, tables,
                      init_barrier, create_table_barrier,
                      system_clock,
                      system_clock_mtx,
                      system_clock_cv,
                      bg_server_clock),
      min_table_staleness_(INT_MAX) { }

  ~SSPAggrBgWorker() { }

protected:
  virtual void SetWaitMsg();
  virtual void PrepareBeforeInfiniteLoop();
  virtual void FinalizeTableStats();
  virtual long ResetBgIdleMilli();
  virtual long BgIdleWork();
  virtual long HandleClockMsg(bool clock_advanced);

  void ReadTableOpLogsIntoOpLogMeta(int32_t table_id,
                                    ClientTable *table);

  size_t ReadTableOpLogMetaUpToClock(
    int32_t table_id, ClientTable *table, int32_t clock_to_push,
    TableOpLogMeta *table_oplog_meta,
    GetSerializedRowOpLogSizeFunc GetSerializedRowOpLogSize,
    BgOpLogPartition *bg_table_oplog);

  size_t ReadTableOpLogMetaUpToCapacity(
    int32_t table_id, ClientTable *table, size_t bytes_accumulated,
    TableOpLogMeta *table_oplog_meta,
    GetSerializedRowOpLogSizeFunc GetSerializedRowOpLogSize,
    BgOpLogPartition *bg_table_oplog);

  size_t ReadTableOpLogMetaUpToClockNoReplay(
      int32_t table_id, ClientTable *table, int32_t clock_to_push,
      TableOpLogMeta *table_oplog_meta,
      RowOpLogSerializer *row_oplog_serializer);

  size_t ReadTableOpLogMetaUpToCapacityNoReplay(
    int32_t table_id, ClientTable *table, size_t bytes_accumulated,
    TableOpLogMeta *table_oplog_meta,
    RowOpLogSerializer *row_oplog_serializer);

  BgOpLogPartition* PrepareOpLogsNormal(
      int32_t table_id, ClientTable *table, int32_t clock_to_push);

  BgOpLogPartition* PrepareOpLogsAppendOnly(
      int32_t table_id, ClientTable *table, int32_t clock_to_push);

  void PrepareOpLogsNormalNoReplay(
      int32_t table_id, ClientTable *table, int32_t clock_to_push);

  void PrepareOpLogsAppendOnlyNoReplay(
      int32_t table_id, ClientTable *table, int32_t clock_to_push);

  BgOpLog *PrepareOpLogsToSend(int32_t clock_to_push);

  BgOpLog *PrepareBgIdleOpLogs();

  BgOpLogPartition* PrepareBgIdleOpLogsNormal(int32_t table_id,
                                              ClientTable *table);

  void PrepareBgIdleOpLogsNormalNoReplay(int32_t table_id,
                                         ClientTable *table);

  BgOpLogPartition *PrepareBgIdleOpLogsAppendOnly(int32_t table_id,
                                                  ClientTable *table);

  void PrepareBgIdleOpLogsAppendOnlyNoReplay(int32_t table_id,
                                     ClientTable *table);

  int32_t min_table_staleness_;

  OpLogMeta oplog_meta_;
  HighResolutionTimer msg_send_timer_;
  double oplog_send_milli_sec_;
};

}
