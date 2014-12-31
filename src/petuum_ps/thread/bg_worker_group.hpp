// author: jinliang
#pragma once

#include <pthread.h>
#include <petuum_ps/thread/abstract_bg_worker.hpp>

namespace petuum {

class BgWorkerGroup {
public:
  BgWorkerGroup(std::map<int32_t, ClientTable* > *tables);
  virtual ~BgWorkerGroup();

  void Start();

  void ShutDown();
  void AppThreadRegister();
  void AppThreadDeregister();

  // Assuming table does not yet exist
  bool CreateTable(int32_t table_id,
                   const ClientTableConfig& table_config);
  void WaitCreateTable();
  bool RequestRow(int32_t table_id, int32_t row_id, int32_t clock);
  void RequestRowAsync(int32_t table_id, int32_t row_id, int32_t clock,
                       bool forced);
  void GetAsyncRowRequestReply();
  void SignalHandleAppendOnlyBuffer(int32_t table_id, int32_t channel_idx);

  void ClockAllTables();
  void SendOpLogsAllTables();

  virtual int32_t GetSystemClock();
  virtual void WaitSystemClock(int32_t my_clock);

protected:
  std::map<int32_t, ClientTable*> *tables_;

  /* Helper Functions */
  std::vector<AbstractBgWorker*> bg_worker_vec_;
  int32_t bg_worker_id_st_;

  pthread_barrier_t init_barrier_;
  pthread_barrier_t create_table_barrier_;

private:
  virtual void CreateBgWorkers();

};

}
