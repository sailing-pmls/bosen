//Author: Jinliang Wei
#pragma once

#include <petuum_ps_common/util/high_resolution_timer.hpp>
#include <petuum_ps_common/include/configs.hpp>
#include <boost/thread/tss.hpp>
#include <boost/unordered_map.hpp>
#include <vector>
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <mutex>
#include <yaml-cpp/yaml.h>

#ifdef PETUUM_STATS

#define STATS_INIT(table_group_config) \
  Stats::Init(table_group_config)

#define STATS_REGISTER_THREAD(thread_type) \
  Stats::RegisterThread(thread_type)

#define STATS_DEREGISTER_THREAD() \
  Stats::DeregisterThread()

#define STATS_APP_LOAD_DATA_BEGIN() \
  petuum::Stats::AppLoadDataBegin()

#define STATS_APP_LOAD_DATA_END() \
  petuum::Stats::AppLoadDataEnd()

#define STATS_APP_INIT_BEGIN() \
  petuum::Stats::AppInitBegin()

#define STATS_APP_INIT_END() \
  petuum::Stats::AppInitEnd()

#define STATS_APP_BOOTSTRAP_BEGIN() \
  petuum::Stats::AppBootstrapBegin()

#define STATS_APP_BOOTSTRAP_END() \
  petuum::Stats::AppBootstrapEnd()

#define STATS_APP_ACCUM_COMP_BEGIN() \
  petuum::Stats::AppAccumCompBegin()

#define STATS_APP_ACCUM_COMP_END() \
  petuum::Stats::AppAccumCompEnd()

#define STATS_APP_ACCUM_OBJ_COMP_BEGIN() \
  petuum::Stats::AppAccumObjCompBegin()

#define STATS_APP_ACCUM_OBJ_COMP_END() \
  petuum::Stats::AppAccumObjCompEnd()

#define STATS_APP_ACCUM_TG_CLOCK_BEGIN() \
  Stats::AppAccumTgClockBegin()

#define STATS_APP_ACCUM_TG_CLOCK_END() \
  Stats::AppAccumTgClockEnd()

#define STATS_APP_SAMPLE_SSP_GET_BEGIN(table_id) \
  Stats::AppSampleSSPGetBegin(table_id)

#define STATS_APP_SAMPLE_SSP_GET_END(table_id, hit) \
  Stats::AppSampleSSPGetEnd(table_id, hit)

#define STATS_APP_ACCUM_SSPPUSH_GET_COMM_BLOCK_BEGIN(table_id) \
  Stats::AppAccumSSPPushGetCommBlockBegin(table_id)

#define STATS_APP_ACCUM_SSPPUSH_GET_COMM_BLOCK_END(table_id) \
  Stats::AppAccumSSPPushGetCommBlockEnd(table_id)

#define STATS_APP_ACCUM_SSP_GET_SERVER_FETCH_BEGIN(table_id) \
  Stats::AppAccumSSPGetServerFetchBegin(table_id)

#define STATS_APP_ACCUM_SSP_GET_SERVER_FETCH_END(table_id) \
  Stats::AppAccumSSPGetServerFetchEnd(table_id)

#define STATS_APP_SAMPLE_INC_BEGIN(table_id) \
  Stats::AppSampleIncBegin(table_id)

#define STATS_APP_SAMPLE_INC_END(table_id) \
  Stats::AppSampleIncEnd(table_id)

#define STATS_APP_SAMPLE_BATCH_INC_BEGIN(table_id) \
  Stats::AppSampleBatchIncBegin(table_id)

#define STATS_APP_SAMPLE_BATCH_INC_END(table_id) \
  Stats::AppSampleBatchIncEnd(table_id)

#define STATS_APP_SAMPLE_BATCH_INC_OPLOG_BEGIN() \
  Stats::AppSampleBatchIncOplogBegin()

#define STATS_APP_SAMPLE_BATCH_INC_OPLOG_END() \
  Stats::AppSampleBatchIncOplogEnd()

#define STATS_APP_SAMPLE_BATCH_INC_PROCESS_STORAGE_BEGIN() \
  Stats::AppSampleBatchIncProcessStorageBegin()

#define STATS_APP_SAMPLE_BATCH_INC_PROCESS_STORAGE_END() \
  Stats::AppSampleBatchIncProcessStorageEnd()

#define STATS_APP_SAMPLE_THREAD_GET_BEGIN(table_id) \
  Stats::AppSampleThreadGetBegin(table_id)

#define STATS_APP_SAMPLE_THREAD_GET_END(table_id) \
  Stats::AppSampleThreadGetEnd(table_id)

#define STATS_APP_SAMPLE_THREAD_INC_BEGIN(table_id) \
  Stats::AppSampleThreadIncBegin(table_id)

#define STATS_APP_SAMPLE_THREAD_INC_END(table_id) \
  Stats::AppSampleThreadIncEnd(table_id)

#define STATS_APP_SAMPLE_THREAD_BATCH_INC_BEGIN(table_id) \
  Stats::AppSampleThreadBatchIncBegin(table_id)

#define STATS_APP_SAMPLE_THREAD_BATCH_INC_END(table_id) \
  Stats::AppSampleThreadBatchIncEnd(table_id)

#define STATS_APP_SAMPLE_CLOCK_BEGIN(table_id) \
  Stats::AppSampleTableClockBegin(table_id)

#define STATS_APP_SAMPLE_CLOCK_END(table_id) \
  Stats::AppSampleTableClockEnd(table_id)

#define STATS_SET_APP_DEFINED_ACCUM_SEC_NAME(name) \
  petuum::Stats::SetAppDefinedAccumSecName(name)

#define STATS_APP_DEFINED_ACCUM_SEC_BEGIN() \
  petuum::Stats::AppDefinedAccumSecBegin()

#define STATS_APP_DEFINED_ACCUM_SEC_END() \
  petuum::Stats::AppDefinedAccumSecEnd()

#define STATS_SET_APP_DEFINED_ACCUM_VAL_NAME(name) \
  petuum::Stats::SetAppDefinedAccumValName(name)

#define STATS_APP_DEFINED_ACCUM_VAL_INC(delta) \
  petuum::Stats::AppDefinedAccumValInc(delta)

#define STATS_APP_ACCUM_APPEND_ONLY_FLUSH_OPLOG_BEGIN() \
  petuum::Stats::AppAccumAppendOnlyFlushOpLogBegin()

#define STATS_APP_ACCUM_APPEND_ONLY_FLUSH_OPLOG_END() \
  petuum::Stats::AppAccumAppendOnlyFlushOpLogEnd()

#define STATS_SET_APP_DEFINED_VEC_NAME(name) \
  petuum::Stats::SetAppDefinedVecName(name)

#define STATS_APPEND_APP_DEFINED_VEC(val) \
  petuum::Stats::AppendAppDefinedVec(val)

#define STATS_BG_ACCUM_OPLOG_SERIALIZE_BEGIN() \
  Stats::BgAccumOpLogSerializeBegin()

#define STATS_BG_ACCUM_OPLOG_SERIALIZE_END() \
  Stats::BgAccumOpLogSerializeEnd()

#define STATS_BG_ACCUM_CLOCK_END_OPLOG_SERIALIZE_BEGIN() \
  Stats::BgAccumClockEndOpLogSerializeBegin()

#define STATS_BG_ACCUM_CLOCK_END_OPLOG_SERIALIZE_END() \
  Stats::BgAccumClockEndOpLogSerializeEnd()

#define STATS_BG_ACCUM_SERVER_PUSH_ROW_APPLY_BEGIN() \
  Stats::BgAccumServerPushRowApplyBegin()

#define STATS_BG_ACCUM_SERVER_PUSH_ROW_APPLY_END() \
  Stats::BgAccumServerPushRowApplyEnd()

#define STATS_BG_CLOCK() \
  Stats::BgClock()

#define STATS_BG_ADD_PER_CLOCK_OPLOG_SIZE(oplog_size) \
  Stats::BgAddPerClockOpLogSize(oplog_size)

#define STATS_BG_ADD_PER_CLOCK_SERVER_PUSH_ROW_SIZE(server_push_row_size) \
  Stats::BgAddPerClockServerPushRowSize(server_push_row_size)

#define STATS_BG_IDLE_INVOKE_INC_ONE() \
  Stats::BgIdleInvokeIncOne()

#define STATS_BG_IDLE_SEND_INC_ONE() \
  Stats::BgIdleSendIncOne()

#define STATS_BG_ACCUM_PUSH_ROW_MSG_RECEIVED_INC_ONE() \
  Stats::BgAccumPushRowMsgReceivedIncOne()

#define STATS_BG_ACCUM_IDLE_SEND_BEGIN() \
  Stats::BgAccumIdleSendBegin()

#define STATS_BG_ACCUM_IDLE_SEND_END() \
  Stats::BgAccumIdleSendEnd()

#define STATS_BG_ACCUM_IDLE_OPLOG_SENT_BYTES(num_bytes) \
  Stats::BgAccumIdleOpLogSentBytes(num_bytes)

#define STATS_BG_ACCUM_SERVER_PUSH_OPLOG_ROW_APPLIED_ADD_ONE() \
  Stats::BgAccumServerPushOpLogRowAppliedAddOne()

#define STATS_BG_ACCUM_SERVER_PUSH_UPDATE_APPLIED_ADD_ONE() \
  Stats::BgAccumServerPushUpdateAppliedAddOne()

#define STATS_BG_ACCUM_SERVER_PUSH_VERSION_DIFF_ADD(diff) \
  Stats::BgAccumServerPushVersionDiffAdd(diff)

#define STATS_BG_SAMPLE_PROCESS_CACHE_INSERT_BEGIN() \
  Stats::BgSampleProcessCacheInsertBegin()

#define STATS_BG_SAMPLE_PROCESS_CACHE_INSERT_END() \
  Stats::BgSampleProcessCacheInsertEnd()

#define STATS_BG_SAMPLE_SERVER_PUSH_DESERIALIZE_BEGIN() \
  Stats::BgSampleServerPushDeserializeBegin()

#define STATS_BG_SAMPLE_SERVER_PUSH_DESERIALIZE_END() \
  Stats::BgSampleServerPushDeserializeEnd()

#define STATS_BG_ACCUM_HANDLE_APPEND_OPLOG_BEGIN() \
  Stats::BgAccumHandleAppendOpLogBegin()

#define STATS_BG_ACCUM_HANDLE_APPEND_OPLOG_END() \
  Stats::BgAccumHandleAppendOpLogEnd()

#define STATS_BG_APPEND_ONLY_CREATE_ROW_OPLOG_INC() \
  Stats::BgAppendOnlyCreateRowOpLogInc()

#define STATS_BG_APPEND_ONLY_RECYCLE_ROW_OPLOG_INC() \
  Stats::BgAppendOnlyRecycleRowOpLogInc()

#define STATS_SERVER_ACCUM_PUSH_ROW_BEGIN() \
  Stats::ServerAccumPushRowBegin()

#define STATS_SERVER_ACCUM_PUSH_ROW_END() \
  Stats::ServerAccumPushRowEnd()

#define STATS_SERVER_ACCUM_APPLY_OPLOG_BEGIN() \
  Stats::ServerAccumApplyOpLogBegin()

#define STATS_SERVER_ACCUM_APPLY_OPLOG_END() \
  Stats::ServerAccumApplyOpLogEnd()

#define STATS_SERVER_CLOCK() \
  Stats::ServerClock()

#define STATS_SERVER_ADD_PER_CLOCK_OPLOG_SIZE(oplog_size) \
  Stats::ServerAddPerClockOpLogSize(oplog_size)

#define STATS_SERVER_ADD_PER_CLOCK_PUSH_ROW_SIZE(push_row_size) \
  Stats::ServerAddPerClockPushRowSize(push_row_size)

#define STATS_SERVER_OPLOG_MSG_RECV_INC_ONE() \
  Stats::ServerOpLogMsgRecvIncOne();

#define STATS_SERVER_PUSH_ROW_MSG_SEND_INC_ONE() \
  Stats::ServerPushRowMsgSendIncOne();

#define STATS_PRINT() \
  Stats::PrintStats()

#else
#define STATS_INIT(table_group_config) ((void) 0)
#define STATS_REGISTER_THREAD(thread_type) ((void) 0)
#define STATS_DEREGISTER_THREAD() ((void) 0)
#define STATS_APP_LOAD_DATA_BEGIN() ((void) 0)
#define STATS_APP_LOAD_DATA_END() ((void) 0)
#define STATS_APP_INIT_BEGIN() ((void) 0)
#define STATS_APP_INIT_END() ((void) 0)
#define STATS_APP_BOOTSTRAP_BEGIN() ((void) 0)
#define STATS_APP_BOOTSTRAP_END() ((void) 0)
#define STATS_APP_ACCUM_COMP_BEGIN() ((void) 0)
#define STATS_APP_ACCUM_COMP_END() ((void) 0)
#define STATS_APP_ACCUM_OBJ_COMP_BEGIN() ((void) 0)
#define STATS_APP_ACCUM_OBJ_COMP_END() ((void) 0)
#define STATS_APP_ACCUM_TG_CLOCK_BEGIN() ((void) 0)
#define STATS_APP_ACCUM_TG_CLOCK_END() ((void) 0)
#define STATS_APP_SAMPLE_SSP_GET_BEGIN(table_id) ((void) 0)
#define STATS_APP_SAMPLE_SSP_GET_END(table_id, hit) ((void) 0)

#define STATS_APP_ACCUM_SSPPUSH_GET_COMM_BLOCK_BEGIN(table_id) \
  ((void) 0)

#define STATS_APP_ACCUM_SSPPUSH_GET_COMM_BLOCK_END(table_id) \
  ((void) 0)

#define STATS_APP_ACCUM_SSP_GET_SERVER_FETCH_BEGIN(table_id) ((void) 0)
#define STATS_APP_ACCUM_SSP_GET_SERVER_FETCH_END(table_id) ((void) 0)
#define STATS_APP_SAMPLE_INC_BEGIN(table_id) ((void) 0)
#define STATS_APP_SAMPLE_INC_END(table_id) ((void) 0)
#define STATS_APP_SAMPLE_BATCH_INC_BEGIN(table_id) ((void) 0)
#define STATS_APP_SAMPLE_BATCH_INC_END(table_id) ((void) 0)
#define STATS_APP_SAMPLE_BATCH_INC_OPLOG_BEGIN() ((void) 0)
#define STATS_APP_SAMPLE_BATCH_INC_OPLOG_END() ((void) 0)
#define STATS_APP_SAMPLE_BATCH_INC_PROCESS_STORAGE_BEGIN() ((void) 0)
#define STATS_APP_SAMPLE_BATCH_INC_PROCESS_STORAGE_END() ((void) 0)
#define STATS_APP_SAMPLE_THREAD_GET_BEGIN(table_id) ((void) 0)
#define STATS_APP_SAMPLE_THREAD_GET_END(table_id) ((void) 0)
#define STATS_APP_SAMPLE_THREAD_INC_BEGIN(table_id) ((void) 0)
#define STATS_APP_SAMPLE_THREAD_INC_END(table_id) ((void) 0)
#define STATS_APP_SAMPLE_THREAD_BATCH_INC_BEGIN(table_id) ((void) 0)
#define STATS_APP_SAMPLE_THREAD_BATCH_INC_END(table_id) ((void) 0)
#define STATS_APP_SAMPLE_CLOCK_BEGIN(table_id) ((void) 0)
#define STATS_APP_SAMPLE_CLOCK_END(table_id) ((void) 0)

#define STATS_SET_APP_DEFINED_ACCUM_SEC_NAME(name) ((void) 0)
#define STATS_APP_DEFINED_ACCUM_SEC_BEGIN() ((void) 0)
#define STATS_APP_DEFINED_ACCUM_SEC_END() ((void) 0)

#define STATS_SET_APP_DEFINED_ACCUM_VAL_NAME(name) ((void) 0)
#define STATS_APP_DEFINED_ACCUM_VAL_INC(delta) ((void) 0)

#define STATS_APP_ACCUM_APPEND_ONLY_FLUSH_OPLOG_BEGIN() ((void) 0)
#define STATS_APP_ACCUM_APPEND_ONLY_FLUSH_OPLOG_END() ((void) 0)

#define STATS_SET_APP_DEFINED_VEC_NAME(name) ((void) 0)
#define STATS_APPEND_APP_DEFINED_VEC(val) ((void) 0)

#define STATS_BG_ACCUM_OPLOG_SERIALIZE_BEGIN() ((void) 0)
#define STATS_BG_ACCUM_OPLOG_SERIALIZE_END() ((void) 0)
#define STATS_BG_ACCUM_CLOCK_END_OPLOG_SERIALIZE_BEGIN() ((void) 0)
#define STATS_BG_ACCUM_CLOCK_END_OPLOG_SERIALIZE_END() ((void) 0)
#define STATS_BG_ACCUM_SERVER_PUSH_ROW_APPLY_BEGIN() ((void) 0)
#define STATS_BG_ACCUM_SERVER_PUSH_ROW_APPLY_END() ((void) 0)
#define STATS_BG_CLOCK() ((void) 0)
#define STATS_BG_ADD_PER_CLOCK_OPLOG_SIZE(oplog_size) ((void) 0)
#define STATS_BG_ADD_PER_CLOCK_SERVER_PUSH_ROW_SIZE(server_push_row_size) \
  ((void) 0)

#define STATS_BG_ACCUM_SERVER_PUSH_OPLOG_ROW_APPLIED_ADD_ONE() \
  ((void) 0)
#define STATS_BG_ACCUM_SERVER_PUSH_UPDATE_APPLIED_ADD_ONE() \
  ((void) 0)
#define STATS_BG_ACCUM_SERVER_PUSH_VERSION_DIFF_ADD(diff) \
  ((void) 0)
#define STATS_BG_SAMPLE_PROCESS_CACHE_INSERT_BEGIN() \
  ((void) 0)
#define STATS_BG_SAMPLE_PROCESS_CACHE_INSERT_END() \
  ((void) 0)
#define STATS_BG_SAMPLE_SERVER_PUSH_DESERIALIZE_BEGIN() \
  ((void) 0)
#define STATS_BG_SAMPLE_SERVER_PUSH_DESERIALIZE_END() \
  ((void) 0)

#define STATS_BG_IDLE_INVOKE_INC_ONE() ((void) 0)
#define STATS_BG_IDLE_SEND_INC_ONE() ((void) 0)
#define STATS_BG_ACCUM_PUSH_ROW_MSG_RECEIVED_INC_ONE() ((void) 0)
#define STATS_BG_ACCUM_IDLE_SEND_BEGIN() ((void) 0)
#define STATS_BG_ACCUM_IDLE_SEND_END() ((void) 0)
#define STATS_BG_ACCUM_IDLE_OPLOG_SENT_BYTES(num_bytes) ((void) 0)

#define STATS_BG_ACCUM_HANDLE_APPEND_OPLOG_BEGIN() ((void) 0)
#define STATS_BG_ACCUM_HANDLE_APPEND_OPLOG_END() ((void) 0)
#define STATS_BG_APPEND_ONLY_CREATE_ROW_OPLOG_INC() ((void) 0)
#define STATS_BG_APPEND_ONLY_RECYCLE_ROW_OPLOG_INC() ((void) 0)

#define STATS_SERVER_ACCUM_PUSH_ROW_BEGIN() ((void) 0)
#define STATS_SERVER_ACCUM_PUSH_ROW_END() ((void) 0)
#define STATS_SERVER_ACCUM_APPLY_OPLOG_BEGIN() ((void) 0)
#define STATS_SERVER_ACCUM_APPLY_OPLOG_END() ((void) 0)
#define STATS_SERVER_CLOCK() ((void) 0)
#define STATS_SERVER_ADD_PER_CLOCK_OPLOG_SIZE(oplog_size) ((void) 0)
#define STATS_SERVER_ADD_PER_CLOCK_PUSH_ROW_SIZE(push_row_size) ((void) 0)
#define STATS_SERVER_OPLOG_MSG_RECV_INC_ONE() ((void) 0)
#define STATS_SERVER_PUSH_ROW_MSG_SEND_INC_ONE() ((void) 0)

#define STATS_PRINT() ((void) 0)
#endif

namespace petuum {

enum ThreadType {
  kAppThread = 0,
  kBgThread = 1,
  kServerThread = 2,
  kNameNodeThread = 3
};

struct AppThreadPerTableStats {
  // System timers:
  HighResolutionTimer get_timer; // ct = client table
  HighResolutionTimer inc_timer;
  HighResolutionTimer batch_inc_timer;

  HighResolutionTimer thread_get_timer;
  HighResolutionTimer thread_inc_timer;
  HighResolutionTimer thread_batch_inc_timer;

  HighResolutionTimer clock_timer;

  HighResolutionTimer ssppush_get_comm_block_timer;

  HighResolutionTimer ssp_get_server_fetch_timer;

  uint64_t num_get;
  uint64_t num_ssp_get_hit;
  uint64_t num_ssp_get_miss;
  uint64_t num_ssp_get_hit_sampled;
  uint64_t num_ssp_get_miss_sampled;
  double accum_sample_ssp_get_hit_sec;
  double accum_sample_ssp_get_miss_sec;

  // Number of Gets that are blocked waiting on receiving
  // server pushed messages.
  uint64_t num_ssppush_get_comm_block;
  // Number of seconds spent on waiting for server push message.
  double accum_ssppush_get_comm_block_sec;

  double accum_ssp_get_server_fetch_sec;

  uint64_t num_inc;
  uint64_t num_inc_sampled;
  double accum_sample_inc_sec;
  uint64_t num_batch_inc;
  uint32_t num_batch_inc_sampled;
  double accum_sample_batch_inc_sec;

  uint64_t num_thread_get;
  double accum_sample_thread_get_sec;
  uint64_t num_thread_inc;
  double accum_sample_thread_inc_sec;
  uint64_t num_thread_batch_inc;
  double accum_sample_thread_batch_inc_sec;

  uint64_t num_clock;
  uint64_t num_clock_sampled;
  double accum_sample_clock_sec;

  AppThreadPerTableStats() :
      num_get(0),
      num_ssp_get_hit(0),
      num_ssp_get_miss(0),
      num_ssp_get_hit_sampled(0),
      num_ssp_get_miss_sampled(0),
      accum_sample_ssp_get_hit_sec(0),
      accum_sample_ssp_get_miss_sec(0),
      num_ssppush_get_comm_block(0),
      accum_ssppush_get_comm_block_sec(0.0),
      accum_ssp_get_server_fetch_sec(0.0),
      num_inc(0),
      num_inc_sampled(0),
      accum_sample_inc_sec(0),
      num_batch_inc(0),
      num_batch_inc_sampled(0),
      accum_sample_batch_inc_sec(0),
      num_thread_get(0),
      accum_sample_thread_get_sec(0),
      num_thread_inc(0),
      accum_sample_thread_inc_sec(0),
      num_thread_batch_inc(0),
      accum_sample_thread_batch_inc_sec(0),
      num_clock(0),
      num_clock_sampled(0),
      accum_sample_clock_sec(0) { }

};

struct AppThreadStats {
  // Application timers:
  HighResolutionTimer thread_life_timer;
  HighResolutionTimer load_data_timer;
  HighResolutionTimer init_timer;
  HighResolutionTimer bootstrap_timer;
  HighResolutionTimer comp_timer;
  HighResolutionTimer obj_comp_timer;

  // System timers:
  HighResolutionTimer tg_clock_timer; // tg = table group
  HighResolutionTimer batch_inc_oplog_timer;
  HighResolutionTimer batch_inc_process_storage_timer;

  boost::unordered_map<int32_t, AppThreadPerTableStats> table_stats;

  double load_data_sec;
  double init_sec;
  double bootstrap_sec;
  double accum_comp_sec;
  double accum_obj_comp_sec;
  double accum_tg_clock_sec;

  // Detailed BatchInc stats (sum over all tables).
  uint64_t num_batch_inc_oplog;
  uint64_t num_batch_inc_process_storage;
  uint32_t num_batch_inc_oplog_sampled;
  uint32_t num_batch_inc_process_storage_sampled;
  double accum_sample_batch_inc_oplog_sec;
  double accum_sample_batch_inc_process_storage_sec;

  double app_defined_accum_sec;
  HighResolutionTimer app_defined_accum_timer;

  double app_defined_accum_val;

  HighResolutionTimer append_only_oplog_flush_timer;
  double accum_append_only_oplog_flush_sec;
  size_t append_only_flush_oplog_count;

  AppThreadStats():
      load_data_sec(0),
      init_sec(0),
      accum_comp_sec(0),
      accum_obj_comp_sec(0),
      accum_tg_clock_sec(0),
      num_batch_inc_oplog(0),
      num_batch_inc_process_storage(0),
      num_batch_inc_oplog_sampled(0),
      num_batch_inc_process_storage_sampled(0),
      accum_sample_batch_inc_oplog_sec(0),
      accum_sample_batch_inc_process_storage_sec(0),
      app_defined_accum_sec(0),
      app_defined_accum_val(0),
      accum_append_only_oplog_flush_sec(0) ,
      append_only_flush_oplog_count(0) { }
};

struct BgThreadStats {
  HighResolutionTimer oplog_serialize_timer;

  double accum_clock_end_oplog_serialize_sec;
  double accum_total_oplog_serialize_sec;

  HighResolutionTimer server_push_row_apply_timer;

  double accum_server_push_row_apply_sec;

  double accum_oplog_sent_kb;
  double accum_server_push_row_recv_kb;

  size_t accum_server_push_oplog_row_applied;
  size_t accum_server_push_update_applied;
  size_t accum_server_push_version_diff;

  HighResolutionTimer process_cache_insert_timer;
  double sample_process_cache_insert_sec;
  size_t num_process_cache_insert;
  size_t num_process_cache_insert_sampled;

  HighResolutionTimer server_push_deserialize_timer;
  double sample_server_push_deserialize_sec;
  size_t num_server_push_deserialize;
  size_t num_server_push_deserialize_sampled;

  std::vector<double> per_clock_oplog_sent_kb;
  std::vector<double> per_clock_server_push_row_recv_kb;

  uint32_t clock_num;

  size_t accum_num_idle_invoke;
  size_t accum_num_idle_send;
  size_t accum_num_push_row_msg_recv;
  double accum_idle_send_sec;

  size_t accum_idle_send_bytes;

  HighResolutionTimer idle_send_timer;

  HighResolutionTimer handle_append_oplog_timer;
  double accum_handle_append_oplog_sec;
  size_t num_append_oplog_buff_handled;

  size_t num_row_oplog_created;
  size_t num_row_oplog_recycled;

  BgThreadStats():
    accum_clock_end_oplog_serialize_sec(0.0),
    accum_total_oplog_serialize_sec(0.0),
    accum_server_push_row_apply_sec(0.0),
    accum_oplog_sent_kb(0.0),
    accum_server_push_row_recv_kb(0.0),
    accum_server_push_oplog_row_applied(0),
    accum_server_push_update_applied(0),
    accum_server_push_version_diff(0),
    sample_process_cache_insert_sec(0.0),
    num_process_cache_insert(0),
    num_process_cache_insert_sampled(0),
    sample_server_push_deserialize_sec(0.0),
    num_server_push_deserialize(0),
    num_server_push_deserialize_sampled(0),
    per_clock_oplog_sent_kb(1, 0.0),
    per_clock_server_push_row_recv_kb(1, 0.0),
    clock_num(0),
    accum_num_idle_invoke(0),
    accum_num_idle_send(0),
    accum_num_push_row_msg_recv(0),
    accum_idle_send_sec(0),
    accum_idle_send_bytes(0),
    accum_handle_append_oplog_sec(0),
    num_row_oplog_created(0),
    num_row_oplog_recycled(0) { }
};

struct ServerThreadStats {
  HighResolutionTimer apply_oplog_timer;
  HighResolutionTimer push_row_timer;

  double accum_apply_oplog_sec;
  double accum_push_row_sec;

  double accum_oplog_recv_kb;
  double accum_push_row_kb;

  std::vector<double> per_clock_oplog_recv_kb;
  std::vector<double> per_clock_push_row_kb;

  uint32_t clock_num;

  size_t accum_num_oplog_msg_recv;
  size_t accum_num_push_row_msg_send;

  ServerThreadStats():
    accum_apply_oplog_sec(0.0),
    accum_push_row_sec(0.0),
    accum_oplog_recv_kb(0.0),
    accum_push_row_kb(0.0),
    per_clock_oplog_recv_kb(1, 0.0),
    per_clock_push_row_kb(1, 0.0),
    clock_num(0),
    accum_num_oplog_msg_recv(0),
    accum_num_push_row_msg_send(0) { }
};

struct NameNodeThreadStats {

};

// Functions are thread-safe unless otherwise specified.
class Stats {
public:
  static void Init(const TableGroupConfig &table_group_config);

  static void RegisterThread(ThreadType thread_type);
  static void DeregisterThread();

  static void AppLoadDataBegin();
  static void AppLoadDataEnd();

  static void AppInitBegin();
  static void AppInitEnd();

  static void AppBootstrapBegin();
  static void AppBootstrapEnd();

  static void AppAccumCompBegin();
  static void AppAccumCompEnd();

  static void AppAccumObjCompBegin();
  static void AppAccumObjCompEnd();

  static void AppAccumTgClockBegin();
  static void AppAccumTgClockEnd();

  static void AppSampleSSPGetBegin(int32_t table_id);
  static void AppSampleSSPGetEnd(int32_t table_id, bool hit);

  static void AppAccumSSPPushGetCommBlockBegin(int32_t table_id);
  static void AppAccumSSPPushGetCommBlockEnd(int32_t table_id);

  static void AppAccumSSPGetServerFetchBegin(int32_t table_id);
  static void AppAccumSSPGetServerFetchEnd(int32_t table_id);

  static void AppSampleIncBegin(int32_t table_id);
  static void AppSampleIncEnd(int32_t table_id);

  static void AppSampleBatchIncBegin(int32_t table_id);
  static void AppSampleBatchIncEnd(int32_t table_id);

  static void AppSampleBatchIncOplogBegin();
  static void AppSampleBatchIncOplogEnd();

  static void AppSampleBatchIncProcessStorageBegin();
  static void AppSampleBatchIncProcessStorageEnd();

  static void AppSampleThreadGetBegin(int32_t table_id);
  static void AppSampleThreadGetEnd(int32_t table_id);

  static void AppSampleThreadIncBegin(int32_t table_id);
  static void AppSampleThreadIncEnd(int32_t table_id);

  static void AppSampleThreadBatchIncBegin(int32_t table_id);
  static void AppSampleThreadBatchIncEnd(int32_t table_id);

  static void AppSampleTableClockBegin(int32_t table_id);
  static void AppSampleTableClockEnd(int32_t table_id);

  static void AppDefinedAccumSecBegin();
  static void AppDefinedAccumSecEnd();

  static void AppAccumAppendOnlyFlushOpLogBegin();
  static void AppAccumAppendOnlyFlushOpLogEnd();

  // the following funcitons are not thread safe
  static void SetAppDefinedAccumSecName(const std::string &name);

  static void SetAppDefinedVecName(const std::string &name);
  static void AppendAppDefinedVec(double val);

  static void SetAppDefinedAccumValName(const std::string &name);
  static void AppDefinedAccumValInc(double delta);

  static void BgAccumOpLogSerializeBegin();
  static void BgAccumOpLogSerializeEnd();

  static void BgAccumClockEndOpLogSerializeBegin();
  static void BgAccumClockEndOpLogSerializeEnd();

  static void BgAccumServerPushRowApplyBegin();
  static void BgAccumServerPushRowApplyEnd();

  static void BgAccumServerPushOpLogRowAppliedAddOne();
  static void BgAccumServerPushUpdateAppliedAddOne();
  static void BgAccumServerPushVersionDiffAdd(size_t diff);

  static void BgSampleProcessCacheInsertBegin();
  static void BgSampleProcessCacheInsertEnd();

  static void BgSampleServerPushDeserializeBegin();
  static void BgSampleServerPushDeserializeEnd();

  static void BgClock();
  static void BgAddPerClockOpLogSize(size_t oplog_size);
  static void BgAddPerClockServerPushRowSize(size_t server_push_row_size);

  static void BgIdleInvokeIncOne();
  static void BgIdleSendIncOne();
  static void BgAccumPushRowMsgReceivedIncOne();
  static void BgAccumIdleSendBegin();
  static void BgAccumIdleSendEnd();
  static void BgAccumIdleOpLogSentBytes(size_t num_bytes);

  static void BgAccumHandleAppendOpLogBegin();
  static void BgAccumHandleAppendOpLogEnd();

  static void BgAppendOnlyCreateRowOpLogInc();
  static void BgAppendOnlyRecycleRowOpLogInc();

  static void ServerAccumPushRowBegin();
  static void ServerAccumPushRowEnd();

  static void ServerAccumApplyOpLogBegin();
  static void ServerAccumApplyOpLogEnd();

  static void ServerClock();
  static void ServerAddPerClockOpLogSize(size_t oplog_size);
  static void ServerAddPerClockPushRowSize(size_t push_row_size);

  static void ServerOpLogMsgRecvIncOne();
  static void ServerPushRowMsgSendIncOne();

  static void PrintStats();

private:

  static void DeregisterAppThread();
  static void DeregisterBgThread();
  static void DeregisterServerThread();

  template<typename T>
  static void YamlPrintSequence(YAML::Emitter *yaml_out,
                                const std::vector<T> &sequence);

  static const int32_t kGetSampleFreq = 10000;
  static const int32_t kIncSampleFreq = 10000;
  static const int32_t kBatchIncSampleFreq = 1000;
  static const int32_t kBatchIncOplogSampleFreq = 10000;
  static const int32_t kBatchIncProcessStorageSampleFreq = 10000;
  static const int32_t kClockSampleFreq = 1;

  static const int32_t kThreadGetSampleFreq = 10000;
  static const int32_t kThreadIncSampleFreq = 10000;
  static const int32_t kThreadBatchIncSampleFreq = 10000;

  static const int32_t kProcessCacheInsertSampleFreq = 1000;
  static const int32_t kServerPushDeserializeSampleFreq = 1000;

  // assuming I have received all server pushed message after this number
  // of Get()s.
  static const int32_t kFirstNGetToSkip = 10;

  static TableGroupConfig table_group_config_;
  static std::string stats_path_;
  static boost::thread_specific_ptr<ThreadType> thread_type_;
  static boost::thread_specific_ptr<AppThreadStats> app_thread_stats_;
  static boost::thread_specific_ptr<BgThreadStats> bg_thread_stats_;
  static boost::thread_specific_ptr<ServerThreadStats> server_thread_stats_;
  static boost::thread_specific_ptr<NameNodeThreadStats>
  name_node_thread_stats_;

  static std::mutex stats_mtx_;

  // App thread stats
  static std::vector<double> app_thread_life_sec_;
  static std::vector<double> app_load_data_sec_;
  static std::vector<double> app_init_sec_;
  static std::vector<double> app_bootstrap_sec_;
  static std::vector<double> app_accum_comp_sec_vec_;

  // including all blocking communication timer per thread.
  static std::vector<double> app_accum_comm_block_sec_;

  // sum over all threads
  static double app_sum_accum_comm_block_sec_;
  // sum over all tables, all threads
  static double app_sum_approx_ssp_get_hit_sec_;
  static double app_sum_approx_inc_sec_;
  static double app_sum_approx_batch_inc_sec_;
  static double app_sum_approx_batch_inc_oplog_sec_;
  static double app_sum_approx_batch_inc_process_storage_sec_;

  static std::string app_defined_accum_sec_name_;
  static std::vector<double> app_defined_accum_sec_vec_;

  static std::string app_defined_vec_name_;
  static std::vector<double> app_defined_vec_;

  static std::string app_defined_accum_val_name_;
  static double app_defined_accum_val_;

  static boost::unordered_map<int32_t, AppThreadPerTableStats> table_stats_;
  static double app_accum_comp_sec_;
  static double app_accum_obj_comp_sec_;
  static double app_accum_tg_clock_sec_;

  static std::vector<double> app_accum_append_only_flush_oplog_sec_;
  static std::vector<size_t> app_append_only_flush_oplog_count_;

  // Bg thread stats
  static double bg_accum_clock_end_oplog_serialize_sec_;
  static double bg_accum_total_oplog_serialize_sec_;

  static double bg_accum_server_push_row_apply_sec_;

  static double bg_accum_oplog_sent_mb_;
  static double bg_accum_server_push_row_recv_mb_;

  static std::vector<double> bg_per_clock_oplog_sent_mb_;
  static std::vector<double> bg_per_clock_server_push_row_recv_mb_;

  static std::vector<size_t> bg_accum_server_push_oplog_row_applied_;
  static std::vector<size_t> bg_accum_server_push_update_applied_;
  static std::vector<size_t> bg_accum_server_push_version_diff_;

  static std::vector<double> bg_sample_process_cache_insert_sec_;
  static std::vector<size_t> bg_num_process_cache_insert_;
  static std::vector<size_t> bg_num_process_cache_insert_sampled_;

  static std::vector<double> bg_sample_server_push_deserialize_sec_;
  static std::vector<size_t> bg_num_server_push_deserialize_;
  static std::vector<size_t> bg_num_server_push_deserialize_sampled_;

  static std::vector<size_t> bg_accum_num_idle_invoke_;
  static std::vector<size_t> bg_accum_num_idle_send_;
  static std::vector<size_t> bg_accum_num_push_row_msg_recv_;
  static std::vector<double> bg_accum_idle_send_sec_;
  static std::vector<size_t> bg_accum_idle_send_bytes_;

  static std::vector<double> bg_accum_handle_append_oplog_sec_;
  static std::vector<size_t> bg_num_append_oplog_buff_handled_;

  static std::vector<size_t> bg_num_row_oplog_created_;
  static std::vector<size_t> bg_num_row_oplog_recycled_;

  // Server thread stats
  static double server_accum_apply_oplog_sec_;

  static double server_accum_push_row_sec_;

  static double server_accum_oplog_recv_mb_;
  static double server_accum_push_row_mb_;

  static std::vector<double> server_per_clock_oplog_recv_mb_;
  static std::vector<double> server_per_clock_push_row_mb_;

  static std::vector<size_t> server_accum_num_oplog_msg_recv_;
  static std::vector<size_t> server_accum_num_push_row_msg_send_;
};

}   // namespace petuum
