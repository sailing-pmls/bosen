#pragma once

#include <pthread.h>
#include <map>
#include <vector>
#include <condition_variable>
#include <boost/unordered_map.hpp>

#include "petuum_ps/include/configs.hpp"
#include "petuum_ps/client/client_table.hpp"
#include "petuum_ps/thread/context.hpp"
#include "petuum_ps/thread/ps_msgs.hpp"
#include "petuum_ps/oplog/oplog_partition.hpp"
#include "petuum_ps/thread/bg_oplog.hpp"
#include "petuum_ps/comm_bus/comm_bus.hpp"
#include "petuum_ps/thread/row_request_oplog_mgr.hpp"
#include "petuum_ps/util/vector_clock.hpp"

namespace petuum {

// Relies on GlobalContext being properly initalized.

class BgWorkers {
public:
  static void Init(std::map<int32_t, ClientTable* > *tables_);

  static void ShutDown();

  static void ThreadRegister();
  static void ThreadDeregister();

  // Assuming table does not yet exist
  static bool CreateTable(int32_t table_id,
      const ClientTableConfig& table_config);
  static void WaitCreateTable();
  static bool RequestRow(int32_t table_id, int32_t row_id, int32_t clock);
  static void RequestRowAsync(int32_t table_id, int32_t row_id, int32_t clock);
  static void GetAsyncRowRequestReply();
  static void ClockAllTables();
  static void SendOpLogsAllTables();

  static int32_t GetSystemClock();
  static void WaitSystemClock(int32_t my_clock);

private:

  struct BgContext {
  public:
    uint32_t version; // version of the data, increment when a set of OpLogs
                       // are sent out; may wrap around
                       // More specifically, version denotes the version of the
                       // OpLogs that haven't been sent out.
    RowRequestOpLogMgr *row_request_oplog_mgr;

    // initialized by BgThreadMain(), used in CreateSendOpLogs()
    // For server x, table y, the size of serialized OpLog is ...
    std::map<int32_t, std::map<int32_t, size_t> > server_table_oplog_size_map;
    // The OpLog msg to each server
    std::map<int32_t, ClientSendOpLogMsg* > server_oplog_msg_map;
    // map server id to oplog msg size
    std::map<int32_t, size_t> server_oplog_msg_size_map;
    // size of oplog per table, reused across multiple tables
    std::map<int32_t, size_t> table_server_oplog_size_map;

    /* Data members needed for server push */
    VectorClock server_vector_clock;
  };

  /* Functions that differentiate SSP, SSPPush and SSPPushValue */
  static void *SSPBgThreadMain(void *thread_id);

  typedef ClientRow *(*CreateClientRowFunc)(int32_t, AbstractRow *);
  typedef void *(*BgThreadMainFunc)(void *);
  static CreateClientRowFunc MyCreateClientRow;

  /* Helper functions*/
  /* Communication functions */
  static void ConnectToNameNodeOrServer(int32_t server_id);
  static void ConnectToBg(int32_t bg_id);
  static void SendToAllLocalBgThreads(void *msg, int32_t size);

  /* Functions for creating ClientRow */
  static ClientRow *CreateSSPClientRow(int32_t clock, AbstractRow *row_data);
  static ClientRow *CreateClientRow(int32_t clock, AbstractRow *row_data);

  static void HandleCreateTables();
  static void BgServerHandshake();

  /* Operate on thread specific BgContext*/
  static void CheckForwardRowRequestToServer(int32_t app_thread_id,
    RowRequestMsg &row_request_msg);
  static void ApplyOpLogsToRowData(int32_t table_id, ClientTable *client_table,
                                   int32_t row_id, uint32_t row_version,
                                   AbstractRow *row_data);
  static void HandleServerRowRequestReply(
      int32_t server_id,
      ServerRowRequestReplyMsg &server_row_request_reply_msg);

  //static void CreateSendOpLogs(BgOpLog *bg_oplog, bool is_clock);
  static void ShutDownClean();

  /* Functions used for SSPPush */
  static void ApplyServerPushedRow(uint32_t version, void *mem,
    size_t mem_size);

  /* Functions for SSPValue */
  static void HandleClockMsg(bool clock_advanced);
  typedef bool (*GetRowOpLogFunc)(TableOpLog &table_oplog, int32_t row_id,
                                  RowOpLog **row_oplog_ptr);
  static GetRowOpLogFunc GetRowOpLog;
  static bool SSPGetRowOpLog(TableOpLog &table_oplog, int32_t row_id,
                             RowOpLog **row_oplog_ptr);
  static bool SSPValueGetRowOpLog(TableOpLog &table_oplog,
                                  int32_t row_id, RowOpLog **row_oplog_ptr);

  static BgOpLog *GetOpLogAndIndex();
  static void CreateOpLogMsgs(const BgOpLog *bg_oplog);

  static std::vector<pthread_t> threads_;
  static std::vector<int32_t> thread_ids_;
  static std::map<int32_t, ClientTable* > *tables_;
  static int32_t id_st_;

  static pthread_barrier_t init_barrier_;
  static pthread_barrier_t create_table_barrier_;

  static boost::thread_specific_ptr<BgContext> bg_context_;
  static CommBus *comm_bus_;

  static CommBus::RecvFunc CommBusRecvAny;
  static CommBus::RecvTimeOutFunc CommBusRecvTimeOutAny;
  static CommBus::SendFunc CommBusSendAny;
  static CommBus::RecvAsyncFunc CommBusRecvAsyncAny;
  static CommBus::RecvWrapperFunc CommBusRecvAnyWrapper;

  static void CommBusRecvAnyBusy(int32_t *sender_id, zmq::message_t *zmq_msg);
  static void CommBusRecvAnySleep(int32_t *sender_id, zmq::message_t *zmq_msg);

  /* Data members needed by server push */
  // Servers might not update all the rows in a client's process cache
  // (there could be a row that nobody writes to for many many clocks).
  // Therefore, we need a locally shared clock to denote the minimum clock that
  // the entire system has reached, which determines if a local thread may read
  // a local cached copy of the row.
  // Each bg worker maintains a vector clock for servers. The clock denotes the
  // clock that the server has reached, that is the minimum clock that the
  // server has seen from all clients.
  // This bg_server_clock_ contains one clock for each bg worker which is the
  // min server clock that the bg worker has seen.

  static std::mutex system_clock_mtx_;
  static std::condition_variable system_clock_cv_;

  static std::atomic_int_fast32_t system_clock_;
  static VectorClockMT bg_server_clock_;
  static boost::unordered_map<int32_t, boost::unordered_map<int32_t, bool> >
  table_oplog_index_;

};

}  // namespace petuum
