// Copyright (c) 2014, Sailing Lab
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the <ORGANIZATION> nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
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
  Stats::AppAcuumTgClockBegin()

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

#define STATS_SERVER_ACCUM_PUSH_ROW_BEGIN() ((void) 0)
#define STATS_SERVER_ACCUM_PUSH_ROW_END() ((void) 0)
#define STATS_SERVER_ACCUM_APPLY_OPLOG_BEGIN() ((void) 0)
#define STATS_SERVER_ACCUM_APPLY_OPLOG_END() ((void) 0)
#define STATS_SERVER_CLOCK() ((void) 0)
#define STATS_SERVER_ADD_PER_CLOCK_OPLOG_SIZE(oplog_size) ((void) 0)
#define STATS_SERVER_ADD_PER_CLOCK_PUSH_ROW_SIZE(push_row_size) ((void) 0)

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

  boost::unordered_map<int32_t, AppThreadPerTableStats> table_stats;

  double load_data_sec;
  double init_sec;
  double bootstrap_sec;
  double accum_comp_sec;
  double accum_obj_comp_sec;
  double accum_tg_clock_sec;

  double app_defined_accum_sec;
  HighResolutionTimer app_defined_accum_timer;

  double app_defined_accum_val;

  AppThreadStats():
      load_data_sec(0),
      init_sec(0),
      accum_comp_sec(0),
      accum_obj_comp_sec(0),
      accum_tg_clock_sec(0),
      app_defined_accum_sec(0),
      app_defined_accum_val(0) { }
};

struct BgThreadStats {
  HighResolutionTimer oplog_serialize_timer;

  double accum_clock_end_oplog_serialize_sec;
  double accum_total_oplog_serialize_sec;

  HighResolutionTimer server_push_row_apply_timer;

  double accum_server_push_row_apply_sec;

  double accum_oplog_sent_kb;
  double accum_server_push_row_recv_kb;

  std::vector<double> per_clock_oplog_sent_kb;
  std::vector<double> per_clock_server_push_row_recv_kb;

  uint32_t clock_num;

  BgThreadStats():
    accum_clock_end_oplog_serialize_sec(0.0),
    accum_total_oplog_serialize_sec(0.0),
    accum_server_push_row_apply_sec(0.0),
    accum_oplog_sent_kb(0.0),
    accum_server_push_row_recv_kb(0.0),
    per_clock_oplog_sent_kb(1, 0.0),
    per_clock_server_push_row_recv_kb(1, 0.0),
    clock_num(0) { }
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

  ServerThreadStats():
    accum_apply_oplog_sec(0.0),
    accum_push_row_sec(0.0),
    accum_oplog_recv_kb(0.0),
    accum_push_row_kb(0.0),
    per_clock_oplog_recv_kb(1, 0.0),
    per_clock_push_row_kb(1, 0.0),
    clock_num(0) { }
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

  static void AppAcuumTgClockBegin();
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

  static void BgClock();
  static void BgAddPerClockOpLogSize(size_t oplog_size);
  static void BgAddPerClockServerPushRowSize(size_t server_push_row_size);

  static void ServerAccumPushRowBegin();
  static void ServerAccumPushRowEnd();

  static void ServerAccumApplyOpLogBegin();
  static void ServerAccumApplyOpLogEnd();

  static void ServerClock();
  static void ServerAddPerClockOpLogSize(size_t oplog_size);
  static void ServerAddPerClockPushRowSize(size_t push_row_size);

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
  static const int32_t kBatchIncSampleFreq = 10000;
  static const int32_t kClockSampleFreq = 1;

  static const int32_t kThreadGetSampleFreq = 10000;
  static const int32_t kThreadIncSampleFreq = 10000;
  static const int32_t kThreadBatchIncSampleFreq = 10000;

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

  // Bg thread stats
  static double bg_accum_clock_end_oplog_serialize_sec_;
  static double bg_accum_total_oplog_serialize_sec_;

  static double bg_accum_server_push_row_apply_sec_;

  static double bg_accum_oplog_sent_mb_;
  static double bg_accum_server_push_row_recv_mb_;

  static std::vector<double> bg_per_clock_oplog_sent_mb_;
  static std::vector<double> bg_per_clock_server_push_row_recv_mb_;

  // Server thread stats
  static double server_accum_apply_oplog_sec_;

  static double server_accum_push_row_sec_;

  static double server_accum_oplog_recv_mb_;
  static double server_accum_push_row_mb_;

  static std::vector<double> server_per_clock_oplog_recv_mb_;
  static std::vector<double> server_per_clock_push_row_mb_;
};

}   // namespace petuum
