// author: jinliang

#pragma once

#include <stdint.h>
#include <map>
#include <vector>
#include <condition_variable>
#include <boost/unordered_map.hpp>

#include <petuum_ps/thread/abstract_bg_worker.hpp>
#include <petuum_ps_common/util/thread.hpp>
#include <petuum_ps/thread/row_request_oplog_mgr.hpp>
#include <petuum_ps/thread/bg_oplog.hpp>
#include <petuum_ps/thread/ps_msgs.hpp>
#include <petuum_ps_common/comm_bus/comm_bus.hpp>
#include <petuum_ps_common/include/configs.hpp>
#include <petuum_ps/client/client_table.hpp>

namespace petuum {
class SSPBgWorker : public AbstractBgWorker {
public:
  SSPBgWorker(int32_t id, int32_t comm_channel_idx,
           std::map<int32_t, ClientTable* > *tables,
           pthread_barrier_t *init_barrier,
           pthread_barrier_t *create_table_barrier);
  virtual ~SSPBgWorker();

protected:
  virtual void CreateRowRequestOpLogMgr();

  virtual bool GetRowOpLog(AbstractOpLog &table_oplog, int32_t row_id,
                           AbstractRowOpLog **row_oplog_ptr);

  /* Functions Called From Main Loop -- BEGIN */
  virtual void PrepareBeforeInfiniteLoop();
  // invoked after all tables have been created
  virtual void FinalizeTableStats();
  virtual long ResetBgIdleMilli();
  virtual long BgIdleWork();
  /* Functions Called From Main Loop -- END */

  virtual ClientRow *CreateClientRow(int32_t clock, AbstractRow *row_data);

  /* Handles Sending OpLogs -- BEGIN */
  virtual BgOpLog *PrepareOpLogsToSend();

  virtual BgOpLogPartition *PrepareOpLogsNormal(int32_t table_id, ClientTable *table);
  virtual BgOpLogPartition *PrepareOpLogsAppendOnly(int32_t table_id, ClientTable *table);

  virtual void PrepareOpLogsNormalNoReplay(int32_t table_id, ClientTable *table);
  virtual void PrepareOpLogsAppendOnlyNoReplay(int32_t table_id, ClientTable *table);

  void TrackBgOpLog(BgOpLog *bg_oplog);
  /* Handles Sending OpLogs -- END */

  /* Handles Row Requests -- BEGIN */
  void CheckAndApplyOldOpLogsToRowData(int32_t table_id,
                                       int32_t row_id, uint32_t row_version,
                                       AbstractRow *row_data);
  void ApplyOldOpLogsToRowData(int32_t table_id,
                               int32_t row_id,
                               uint32_t version_st,
                               uint32_t version_end,
                               AbstractRow *row_data);
  /* Handles Row Requests -- END */
};

}
