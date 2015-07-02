// author: jinliang

#pragma once

#include <petuum_ps/thread/bg_worker_group.hpp>

namespace petuum {

class BgWorkers {
public:
  static void Start(std::map<int32_t, ClientTable* > *tables);

  static void ShutDown();
  static void AppThreadRegister();
  static void AppThreadDeregister();

  // Assuming table does not yet exist
  static bool CreateTable(int32_t table_id,
                          const ClientTableConfig& table_config);
  static void WaitCreateTable();
  static bool RequestRow(int32_t table_id, int32_t row_id, int32_t clock);
  // If forced is set to true, a row request is forced to send even if
  // it exists in the process storage and clock is fresh enough.
  static void RequestRowAsync(int32_t table_id, int32_t row_id, int32_t clock,
                              bool forced);
  static void GetAsyncRowRequestReply();
  static void SignalHandleAppendOnlyBuffer(int32_t table_id, int32_t channel_idx);
  static void ClockAllTables();
  static void SendOpLogsAllTables();

  static int32_t GetSystemClock();
  static void WaitSystemClock(int32_t my_clock);

private:
  static BgWorkerGroup *bg_worker_group_;
};

}
