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

#include <petuum_ps_common/util/stats.hpp>
#include <petuum_ps_common/include/constants.hpp>
#include <glog/logging.h>
#include <sstream>
#include <fstream>

namespace petuum {
TableGroupConfig Stats::table_group_config_;
std::string Stats::stats_path_;
boost::thread_specific_ptr<ThreadType> Stats::thread_type_;
boost::thread_specific_ptr<AppThreadStats> Stats::app_thread_stats_;
boost::thread_specific_ptr<BgThreadStats> Stats::bg_thread_stats_;
boost::thread_specific_ptr<ServerThreadStats> Stats::server_thread_stats_;
boost::thread_specific_ptr<NameNodeThreadStats> Stats::name_node_thread_stats_;

std::mutex Stats::stats_mtx_;
std::vector<double> Stats::app_thread_life_sec_;
std::vector<double> Stats::app_load_data_sec_;
std::vector<double> Stats::app_init_sec_;
std::vector<double> Stats::app_bootstrap_sec_;
std::vector<double> Stats::app_accum_comp_sec_vec_;

std::vector<double> Stats::app_accum_comm_block_sec_;

double Stats::app_sum_accum_comm_block_sec_ = 0;
double Stats::app_sum_approx_ssp_get_hit_sec_ = 0;
double Stats::app_sum_approx_inc_sec_ = 0;
double Stats::app_sum_approx_batch_inc_sec_ = 0;

std::string Stats::app_defined_accum_sec_name_;
std::vector<double> Stats::app_defined_accum_sec_vec_;

std::string Stats::app_defined_vec_name_;
std::vector<double> Stats::app_defined_vec_;

std::string Stats::app_defined_accum_val_name_;
double Stats::app_defined_accum_val_;

boost::unordered_map<int32_t, AppThreadPerTableStats> Stats::table_stats_;
double Stats::app_accum_comp_sec_ = 0;
double Stats::app_accum_obj_comp_sec_ = 0;
double Stats::app_accum_tg_clock_sec_ = 0;

double Stats::bg_accum_clock_end_oplog_serialize_sec_ = 0;
double Stats::bg_accum_total_oplog_serialize_sec_ = 0;
double Stats::bg_accum_server_push_row_apply_sec_ = 0;
double Stats::bg_accum_oplog_sent_mb_ = 0;
double Stats::bg_accum_server_push_row_recv_mb_ = 0;

std::vector<double> Stats::bg_per_clock_oplog_sent_mb_;
std::vector<double> Stats::bg_per_clock_server_push_row_recv_mb_;

double Stats::server_accum_apply_oplog_sec_;
double Stats::server_accum_push_row_sec_;

double Stats::server_accum_oplog_recv_mb_;
double Stats::server_accum_push_row_mb_;

std::vector<double> Stats::server_per_clock_oplog_recv_mb_;
std::vector<double> Stats::server_per_clock_push_row_mb_;

void Stats::Init(const TableGroupConfig &table_group_config) {
  table_group_config_ = table_group_config;

  std::stringstream stats_path_ss;
  stats_path_ss << table_group_config.stats_path;
  stats_path_ss << "." << table_group_config.client_id;

  stats_path_ = stats_path_ss.str();
}

void Stats::RegisterThread(ThreadType thread_type) {

  thread_type_.reset(new ThreadType);
  *thread_type_ = thread_type;
  switch (thread_type) {
    case kAppThread:
      app_thread_stats_.reset(new AppThreadStats);
      break;
    case kBgThread:
      bg_thread_stats_.reset(new BgThreadStats);
      break;
    case kServerThread:
      server_thread_stats_.reset(new ServerThreadStats);
      break;
    case kNameNodeThread:
      name_node_thread_stats_.reset(new NameNodeThreadStats);
      break;
    default:
      LOG(FATAL) << "Unrecognized thread type " << thread_type;
  }
}

void Stats::DeregisterThread() {
  switch(*thread_type_) {
    case kAppThread:
      DeregisterAppThread();
      break;
    case kBgThread:
      DeregisterBgThread();
      break;
    case kServerThread:
      DeregisterServerThread();
      break;
    case kNameNodeThread:
      break;
    default:
      LOG(FATAL) << "Unrecognized thread type " << *thread_type_;
  }
}

void Stats::DeregisterAppThread() {
  std::lock_guard<std::mutex> lock(stats_mtx_);
  app_thread_life_sec_.push_back(
      app_thread_stats_->thread_life_timer.elapsed());

  if (app_thread_stats_->load_data_sec > 0.0)
    app_load_data_sec_.push_back(app_thread_stats_->load_data_sec);

  if (app_thread_stats_->init_sec > 0.0)
    app_init_sec_.push_back(app_thread_stats_->init_sec);

  if (app_thread_stats_->bootstrap_sec > 0.0)
    app_bootstrap_sec_.push_back(app_thread_stats_->bootstrap_sec);

  if (app_thread_stats_->accum_comp_sec > 0.0)
    app_accum_comp_sec_vec_.push_back(app_thread_stats_->accum_comp_sec);

  app_defined_accum_sec_vec_.push_back(app_thread_stats_->app_defined_accum_sec);

  app_defined_accum_val_ += app_thread_stats_->app_defined_accum_val;

  app_accum_comp_sec_ += app_thread_stats_->accum_comp_sec;
  app_accum_obj_comp_sec_ += app_thread_stats_->accum_obj_comp_sec;
  app_accum_tg_clock_sec_ += app_thread_stats_->accum_tg_clock_sec;

  double my_accum_comm_block_sec = 0.0;

  for (auto table_stats_iter = app_thread_stats_->table_stats.begin();
       table_stats_iter != app_thread_stats_->table_stats.end();
       table_stats_iter++) {

    int32_t table_id = table_stats_iter->first;
    AppThreadPerTableStats &thread_table_stats = table_stats_iter->second;

    table_stats_[table_id].num_get += thread_table_stats.num_get;

    table_stats_[table_id].num_ssp_get_hit
        += thread_table_stats.num_ssp_get_hit;
    table_stats_[table_id].num_ssp_get_miss
        += thread_table_stats.num_ssp_get_miss;

    table_stats_[table_id].num_ssp_get_hit_sampled
        += thread_table_stats.num_ssp_get_hit_sampled;

    table_stats_[table_id].num_ssp_get_miss_sampled
        += thread_table_stats.num_ssp_get_miss_sampled;

    table_stats_[table_id].accum_sample_ssp_get_hit_sec
        += thread_table_stats.accum_sample_ssp_get_hit_sec;

    table_stats_[table_id].accum_sample_ssp_get_miss_sec
        += thread_table_stats.accum_sample_ssp_get_miss_sec;

    table_stats_[table_id].num_ssppush_get_comm_block
      += thread_table_stats.num_ssppush_get_comm_block;

    table_stats_[table_id].accum_ssppush_get_comm_block_sec
      += thread_table_stats.accum_ssppush_get_comm_block_sec;

    my_accum_comm_block_sec
      += thread_table_stats.accum_ssppush_get_comm_block_sec;

    table_stats_[table_id].accum_ssp_get_server_fetch_sec
      += thread_table_stats.accum_ssp_get_server_fetch_sec;

    my_accum_comm_block_sec
      += thread_table_stats.accum_ssp_get_server_fetch_sec;

    table_stats_[table_id].num_inc += thread_table_stats.num_inc;

    table_stats_[table_id].num_inc_sampled
      += thread_table_stats.num_inc_sampled;

    table_stats_[table_id].accum_sample_inc_sec
        += thread_table_stats.accum_sample_inc_sec;

    table_stats_[table_id].num_batch_inc
        += thread_table_stats.num_batch_inc;

    table_stats_[table_id].num_batch_inc_sampled
      += thread_table_stats.num_batch_inc_sampled;

    table_stats_[table_id].accum_sample_batch_inc_sec
        += thread_table_stats.accum_sample_batch_inc_sec;

    table_stats_[table_id].num_thread_get += thread_table_stats.num_thread_get;
    table_stats_[table_id].accum_sample_thread_get_sec
      += table_stats_[table_id].accum_sample_thread_get_sec;

    table_stats_[table_id].num_thread_inc += thread_table_stats.num_thread_inc;

    table_stats_[table_id].accum_sample_thread_inc_sec
        += thread_table_stats.accum_sample_thread_inc_sec;

    table_stats_[table_id].num_thread_batch_inc
        += thread_table_stats.num_thread_batch_inc;

    table_stats_[table_id].accum_sample_thread_batch_inc_sec
        += thread_table_stats.accum_sample_thread_batch_inc_sec;

    table_stats_[table_id].num_clock += thread_table_stats.num_clock;

    table_stats_[table_id].num_clock_sampled
      += thread_table_stats.num_clock_sampled;

    table_stats_[table_id].accum_sample_clock_sec
        += thread_table_stats.accum_sample_clock_sec;
  }

  app_accum_comm_block_sec_.push_back(my_accum_comm_block_sec);
  app_sum_accum_comm_block_sec_ += my_accum_comm_block_sec;
}

void Stats::DeregisterBgThread() {
  std::lock_guard<std::mutex> lock(stats_mtx_);
  BgThreadStats &stats = *bg_thread_stats_;
  bg_accum_clock_end_oplog_serialize_sec_
    += stats.accum_clock_end_oplog_serialize_sec;

  bg_accum_total_oplog_serialize_sec_
    += stats.accum_total_oplog_serialize_sec;

  bg_accum_server_push_row_apply_sec_
    += stats.accum_server_push_row_apply_sec;

  bg_accum_oplog_sent_mb_
    += stats.accum_oplog_sent_kb / double(kTwoToPowerTen);

  bg_accum_server_push_row_recv_mb_
    += stats.accum_server_push_row_recv_kb / double(kTwoToPowerTen);

  size_t vec_size = stats.per_clock_oplog_sent_kb.size();

  if (bg_per_clock_oplog_sent_mb_.size() < vec_size) {
    bg_per_clock_oplog_sent_mb_.resize(vec_size, 0.0);
  }

  for (int i = 0; i < vec_size; ++i) {
    bg_per_clock_oplog_sent_mb_[i]
      += stats.per_clock_oplog_sent_kb[i] / double(kTwoToPowerTen);
  }

  vec_size = stats.per_clock_server_push_row_recv_kb.size();

  if (bg_per_clock_server_push_row_recv_mb_.size() < vec_size) {
    bg_per_clock_server_push_row_recv_mb_.resize(vec_size, 0.0);
  }

  for (int i = 0; i < vec_size; ++i) {
    bg_per_clock_server_push_row_recv_mb_[i]
      += stats.per_clock_server_push_row_recv_kb[i] / double(kTwoToPowerTen);
  }
}

void Stats::DeregisterServerThread() {
  std::lock_guard<std::mutex> lock(stats_mtx_);
  ServerThreadStats &stats = *server_thread_stats_;
  server_accum_apply_oplog_sec_
    += stats.accum_apply_oplog_sec;

  server_accum_push_row_sec_
    += stats.accum_push_row_sec;

  server_accum_oplog_recv_mb_
    += stats.accum_oplog_recv_kb / double(kTwoToPowerTen);

  server_accum_push_row_mb_
    += stats.accum_push_row_kb / double(kTwoToPowerTen);

  size_t vec_size = stats.per_clock_oplog_recv_kb.size();

  if (server_per_clock_oplog_recv_mb_.size() < vec_size) {
    server_per_clock_oplog_recv_mb_.resize(vec_size, 0.0);
  }

  for (int i = 0; i < vec_size; ++i) {
    server_per_clock_oplog_recv_mb_[i]
      += stats.per_clock_oplog_recv_kb[i] / double(kTwoToPowerTen);
  }

  vec_size = stats.per_clock_push_row_kb.size();

  if (server_per_clock_push_row_mb_.size() < vec_size) {
    server_per_clock_push_row_mb_.resize(vec_size, 0.0);
  }

  for (int i = 0; i < vec_size; ++i) {
    server_per_clock_push_row_mb_[i]
      += stats.per_clock_push_row_kb[i] / double(kTwoToPowerTen);
  }
}

void Stats::AppLoadDataBegin() {
  app_thread_stats_->load_data_timer.restart();
}

void Stats::AppLoadDataEnd() {
  AppThreadStats &stats = *app_thread_stats_;
  stats.load_data_sec = stats.load_data_timer.elapsed();
}

void Stats::AppInitBegin() {
  app_thread_stats_->init_timer.restart();
}
void Stats::AppInitEnd() {
  AppThreadStats &stats = *app_thread_stats_;
  stats.init_sec = stats.init_timer.elapsed();
}

void Stats::AppBootstrapBegin() {
  app_thread_stats_->bootstrap_timer.restart();
}

void Stats::AppBootstrapEnd() {
  AppThreadStats &stats = *app_thread_stats_;
  stats.bootstrap_sec = stats.bootstrap_timer.elapsed();
}

void Stats::AppAccumCompBegin() {
  app_thread_stats_->comp_timer.restart();
}

void Stats::AppAccumCompEnd() {
  AppThreadStats &stats = *app_thread_stats_;
  stats.accum_comp_sec += stats.comp_timer.elapsed();
}

void Stats::AppAccumObjCompBegin() {
  app_thread_stats_->obj_comp_timer.restart();
}

void Stats::AppAccumObjCompEnd() {
  AppThreadStats &stats = *app_thread_stats_;
  stats.accum_obj_comp_sec += stats.obj_comp_timer.elapsed();
}

void Stats::AppAcuumTgClockBegin() {
  app_thread_stats_->tg_clock_timer.restart();
}

void Stats::AppAccumTgClockEnd() {
  AppThreadStats &stats = *app_thread_stats_;
  stats.accum_tg_clock_sec += stats.tg_clock_timer.elapsed();
}

void Stats::AppSampleSSPGetBegin(int32_t table_id) {
  AppThreadStats &stats = *app_thread_stats_;

  if ((stats.table_stats[table_id].num_get < kFirstNGetToSkip)
       || (stats.table_stats[table_id].num_get % kGetSampleFreq))
    return;

  stats.table_stats[table_id].get_timer.restart();
}

void Stats::AppSampleSSPGetEnd(int32_t table_id, bool hit) {
  AppThreadStats &stats = *app_thread_stats_;

  uint64_t org_num_get = stats.table_stats[table_id].num_get;
  ++stats.table_stats[table_id].num_get;

  if (hit)
    ++stats.table_stats[table_id].num_ssp_get_hit;
  else
    ++stats.table_stats[table_id].num_ssp_get_miss;

  if ((org_num_get - 1) < kFirstNGetToSkip
      || ((org_num_get - 1) % kGetSampleFreq)) {
    return;
  }

  if (hit) {
    stats.table_stats[table_id].accum_sample_ssp_get_hit_sec
        = stats.table_stats[table_id].get_timer.elapsed();
    ++stats.table_stats[table_id].num_ssp_get_hit_sampled;
  } else {
    stats.table_stats[table_id].accum_sample_ssp_get_miss_sec
        = stats.table_stats[table_id].get_timer.elapsed();
    ++stats.table_stats[table_id].num_ssp_get_miss_sampled;
  }
}

void Stats::AppAccumSSPPushGetCommBlockBegin(int32_t table_id) {
  app_thread_stats_->table_stats[table_id].ssppush_get_comm_block_timer.restart();
}

void Stats::AppAccumSSPPushGetCommBlockEnd(int32_t table_id) {
  AppThreadStats &stats = *app_thread_stats_;
  stats.table_stats[table_id].accum_ssppush_get_comm_block_sec
    += stats.table_stats[table_id].ssppush_get_comm_block_timer.elapsed();

  ++(stats.table_stats[table_id].num_ssppush_get_comm_block);
}

void Stats::AppAccumSSPGetServerFetchBegin(int32_t table_id) {
  app_thread_stats_->table_stats[table_id].ssp_get_server_fetch_timer.restart();
}

void Stats::AppAccumSSPGetServerFetchEnd(int32_t table_id) {
  AppThreadStats &stats = *app_thread_stats_;
  stats.table_stats[table_id].accum_ssp_get_server_fetch_sec
    += stats.table_stats[table_id].ssp_get_server_fetch_timer.elapsed();
}

void Stats::AppSampleIncBegin(int32_t table_id) {
  AppThreadStats &stats = *app_thread_stats_;

  if (stats.table_stats[table_id].num_inc % kIncSampleFreq)
    return;

  stats.table_stats[table_id].inc_timer.restart();
}

void Stats::AppSampleIncEnd(int32_t table_id) {
  AppThreadStats &stats = *app_thread_stats_;

  uint64_t org_num_inc = stats.table_stats[table_id].num_inc;
  ++stats.table_stats[table_id].num_inc;
  if (org_num_inc % kIncSampleFreq)
    return;

  stats.table_stats[table_id].accum_sample_inc_sec
      += stats.table_stats[table_id].inc_timer.elapsed();
  ++stats.table_stats[table_id].num_inc_sampled;
}

void Stats::AppSampleBatchIncBegin(int32_t table_id) {
  AppThreadStats &stats = *app_thread_stats_;

  if (stats.table_stats[table_id].num_batch_inc % kBatchIncSampleFreq)
    return;

  stats.table_stats[table_id].batch_inc_timer.restart();
}

void Stats::AppSampleBatchIncEnd(int32_t table_id) {
  AppThreadStats &stats = *app_thread_stats_;

  uint64_t org_num_batch_inc = stats.table_stats[table_id].num_batch_inc;
  ++stats.table_stats[table_id].num_batch_inc;
  if (org_num_batch_inc % kBatchIncSampleFreq)
    return;

  stats.table_stats[table_id].accum_sample_batch_inc_sec
      += stats.table_stats[table_id].batch_inc_timer.elapsed();
  ++stats.table_stats[table_id].num_batch_inc_sampled;
}

void Stats::AppSampleTableClockBegin(int32_t table_id) {
  AppThreadStats &stats = *app_thread_stats_;

  if (stats.table_stats[table_id].num_clock % kClockSampleFreq)
    return;

  stats.table_stats[table_id].clock_timer.restart();
}

void Stats::AppSampleTableClockEnd(int32_t table_id) {
  AppThreadStats &stats = *app_thread_stats_;

  uint64_t org_num_clock = stats.table_stats[table_id].num_clock;
  ++stats.table_stats[table_id].num_clock;

  if (org_num_clock % kClockSampleFreq)
    return;

  stats.table_stats[table_id].accum_sample_clock_sec
      += stats.table_stats[table_id].clock_timer.elapsed();
  ++stats.table_stats[table_id].num_clock_sampled;
}

void Stats::AppSampleThreadGetBegin(int32_t table_id) {
  AppThreadStats &stats = *app_thread_stats_;

  if (stats.table_stats[table_id].num_thread_get % kThreadGetSampleFreq)
    return;

  stats.table_stats[table_id].thread_get_timer.restart();
}

void Stats::AppSampleThreadGetEnd(int32_t table_id) {
  AppThreadStats &stats = *app_thread_stats_;

  uint64_t org_num_thread_get = stats.table_stats[table_id].num_thread_get;
  ++stats.table_stats[table_id].num_thread_get;

  if (org_num_thread_get % kThreadGetSampleFreq)
    return;

  stats.table_stats[table_id].accum_sample_thread_get_sec
      += stats.table_stats[table_id].thread_get_timer.elapsed();
}

void Stats::SetAppDefinedAccumSecName(const std::string &name) {
  app_defined_accum_sec_name_ = name;
}

void Stats::AppDefinedAccumSecBegin() {
  app_thread_stats_->app_defined_accum_timer.restart();
}

void Stats::AppDefinedAccumSecEnd() {
  AppThreadStats &stats = *app_thread_stats_;

  stats.app_defined_accum_sec += stats.app_defined_accum_timer.elapsed();
}

void Stats::SetAppDefinedVecName(const std::string &name) {
  app_defined_vec_name_ = name;
}

void Stats::AppendAppDefinedVec(double val) {
  app_defined_vec_.push_back(val);
}

void Stats::SetAppDefinedAccumValName(const std::string &name) {
  app_defined_accum_val_name_ = name;
}

void Stats::AppDefinedAccumValInc(double val) {
  app_thread_stats_->app_defined_accum_val += val;
}

void Stats::BgAccumOpLogSerializeBegin() {
  bg_thread_stats_->oplog_serialize_timer.restart();
}

void Stats::BgAccumOpLogSerializeEnd() {
  BgThreadStats& stats = *bg_thread_stats_;
  stats.accum_total_oplog_serialize_sec += stats.oplog_serialize_timer.elapsed();
}

void Stats::BgAccumClockEndOpLogSerializeBegin() {
  bg_thread_stats_->oplog_serialize_timer.restart();
}

void Stats::BgAccumClockEndOpLogSerializeEnd() {
  BgThreadStats& stats = *bg_thread_stats_;

  double elapsed = stats.oplog_serialize_timer.elapsed();

  stats.accum_total_oplog_serialize_sec += elapsed;
  stats.accum_clock_end_oplog_serialize_sec += elapsed;
}

void Stats::BgAccumServerPushRowApplyBegin() {
  bg_thread_stats_->server_push_row_apply_timer.restart();
}

void Stats::BgAccumServerPushRowApplyEnd() {
  BgThreadStats &stats = *bg_thread_stats_;

  stats.accum_server_push_row_apply_sec
    += stats.server_push_row_apply_timer.elapsed();
}

void Stats::BgClock() {
  ++(bg_thread_stats_->clock_num);
  bg_thread_stats_->per_clock_oplog_sent_kb.push_back(0.0);
  bg_thread_stats_->per_clock_server_push_row_recv_kb.push_back(0.0);
}

void Stats::BgAddPerClockOpLogSize(size_t oplog_size) {
  double oplog_size_kb
    = double(oplog_size) / double(kTwoToPowerTen);

  BgThreadStats &stats = *bg_thread_stats_;

  stats.per_clock_oplog_sent_kb[stats.clock_num]
    += oplog_size_kb;
  stats.accum_oplog_sent_kb += oplog_size_kb;
}

void Stats::BgAddPerClockServerPushRowSize(size_t server_push_row_size) {
  double server_push_row_size_kb
    = double(server_push_row_size) / double(kTwoToPowerTen);

  BgThreadStats &stats = *bg_thread_stats_;

  stats.per_clock_server_push_row_recv_kb[stats.clock_num]
    += server_push_row_size_kb;
  stats.accum_server_push_row_recv_kb += server_push_row_size_kb;
}

void Stats::ServerAccumApplyOpLogBegin() {
  server_thread_stats_->apply_oplog_timer.restart();
}

void Stats::ServerAccumApplyOpLogEnd() {
  ServerThreadStats &stats = *server_thread_stats_;

  stats.accum_apply_oplog_sec
    += stats.apply_oplog_timer.elapsed();
}

void Stats::ServerAccumPushRowBegin() {
  server_thread_stats_->push_row_timer.restart();
}

void Stats::ServerAccumPushRowEnd() {
  ServerThreadStats &stats = *server_thread_stats_;

  stats.accum_push_row_sec
    += stats.push_row_timer.elapsed();
}

void Stats::ServerClock() {
  ++server_thread_stats_->clock_num;
  server_thread_stats_->per_clock_oplog_recv_kb.push_back(0.0);
  server_thread_stats_->per_clock_push_row_kb.push_back(0.0);
}

void Stats::ServerAddPerClockOpLogSize(size_t oplog_size) {
  double oplog_size_kb
    = double(oplog_size) / double(kTwoToPowerTen);

  ServerThreadStats &stats = *server_thread_stats_;

  stats.per_clock_oplog_recv_kb[stats.clock_num]
    += oplog_size_kb;
  stats.accum_oplog_recv_kb += oplog_size_kb;
}

void Stats::ServerAddPerClockPushRowSize(size_t push_row_size) {
  double push_row_size_kb
    = double(push_row_size) / double(kTwoToPowerTen);

  ServerThreadStats &stats = *server_thread_stats_;

  stats.per_clock_push_row_kb[stats.clock_num]
    += push_row_size_kb;
  stats.accum_push_row_kb += push_row_size_kb;
}

template<typename T>
void Stats::YamlPrintSequence(YAML::Emitter *yaml_out,
                              const std::vector<T> &sequence) {
  *yaml_out << YAML::Flow
            << YAML::BeginSeq;
  for (auto seq_iter = sequence.begin(); seq_iter != sequence.end();
       seq_iter++) {
    *yaml_out << *seq_iter;
  }

  *yaml_out << YAML::EndSeq;
}

void Stats::PrintStats() {
  YAML::Emitter yaml_out;
  std::lock_guard<std::mutex> lock(stats_mtx_);

  yaml_out << YAML::BeginMap
           << YAML::Comment("Table Group Configuration")
           << YAML::Key << "num_total_server_threads"
           << YAML::Value << table_group_config_.num_total_server_threads
           << YAML::Key << "num_tables"
           << YAML::Value << table_group_config_.num_tables
           << YAML::Key << "num_total_clients"
           << YAML::Value << table_group_config_.num_total_clients
           << YAML::Key << "num_local_server_threads"
           << YAML::Value << table_group_config_.num_local_server_threads
           << YAML::Key << "num_local_app_threads"
           << YAML::Value << table_group_config_.num_local_app_threads
           << YAML::Key << "num_local_bg_threads"
           << YAML::Value << table_group_config_.num_local_bg_threads
           << YAML::Key << "client_id"
           << YAML::Value << table_group_config_.client_id
           << YAML::Key << "aggressive_clock"
           << YAML::Value << table_group_config_.aggressive_clock
           << YAML::Key << "consistency_model"
           << YAML::Value << table_group_config_.consistency_model
           << YAML::Key << "aggressive_cpu"
           << YAML::Value << table_group_config_.aggressive_cpu
           << YAML::EndMap;

    for (auto table_stats_iter = table_stats_.begin();
       table_stats_iter != table_stats_.end(); table_stats_iter++) {

      double approx_ssp_get_hit_sec = 0;
      if (table_stats_iter->second.num_ssp_get_hit_sampled != 0) {
	approx_ssp_get_hit_sec = double(table_stats_iter->second.num_ssp_get_hit)
	  / double(table_stats_iter->second.num_ssp_get_hit_sampled)
	  * table_stats_iter->second.accum_sample_ssp_get_hit_sec;
      }

      app_sum_approx_ssp_get_hit_sec_ += approx_ssp_get_hit_sec;

      double approx_inc_sec = 0;
      if (table_stats_iter->second.num_inc_sampled != 0) {
	approx_inc_sec = double(table_stats_iter->second.num_inc)
	  / double(table_stats_iter->second.num_inc_sampled)
	  * table_stats_iter->second.accum_sample_inc_sec;
      }
      app_sum_approx_inc_sec_ += approx_inc_sec;

      double approx_batch_inc_sec = 0;
      if (table_stats_iter->second.num_batch_inc_sampled != 0) {
	approx_batch_inc_sec = double(table_stats_iter->second.num_batch_inc)
	  / double(table_stats_iter->second.num_batch_inc_sampled)
	  * table_stats_iter->second.accum_sample_batch_inc_sec;
      }
      app_sum_approx_batch_inc_sec_ += approx_batch_inc_sec;
    }

  yaml_out << YAML::BeginMap
           << YAML::Comment("Application Statistics");
  yaml_out << YAML::Key << "app_thread_life_sec"
           << YAML::Value;
  YamlPrintSequence(&yaml_out, app_thread_life_sec_);
  yaml_out << YAML::Key << "app_load_data_sec"
           << YAML::Value;
  YamlPrintSequence(&yaml_out, app_load_data_sec_);
  yaml_out << YAML::Key << "app_init_sec"
           << YAML::Value;
  YamlPrintSequence(&yaml_out, app_init_sec_);
  yaml_out << YAML::Key << "app_bootstrap_sec"
           << YAML::Value;
  YamlPrintSequence(&yaml_out, app_bootstrap_sec_);
  yaml_out << YAML::Key << "app_accum_comp_sec_vec"
           << YAML::Value;
  YamlPrintSequence(&yaml_out, app_accum_comp_sec_vec_);

  yaml_out << YAML::Key << "app_accum_comm_block_sec"
	   << YAML::Value;
  YamlPrintSequence(&yaml_out, app_accum_comm_block_sec_);
  {
    std::stringstream app_defined_accum_sec_ss;
    app_defined_accum_sec_ss << "app_defined:";
    app_defined_accum_sec_ss << app_defined_accum_sec_name_;

    yaml_out << YAML::Key << app_defined_accum_sec_ss.str()
	     << YAML::Value;
    YamlPrintSequence(&yaml_out, app_defined_accum_sec_vec_);
  }

  {
    std::stringstream app_defined_accum_val_ss;
    app_defined_accum_val_ss << "app_defined:";
    app_defined_accum_val_ss << app_defined_accum_val_name_;

    yaml_out << YAML::Key << app_defined_accum_val_ss.str()
	     << YAML::Value
	     << app_defined_accum_val_;
  }

  yaml_out << YAML::Key << "total_comp_thread"
	   << YAML::Value << app_accum_comp_sec_vec_.size();

  yaml_out << YAML::Key << "app_accum_comp_sec"
	   << YAML::Value << app_accum_comp_sec_;

  yaml_out << YAML::Key << "app_sum_accum_comm_block_sec"
	   << YAML::Value << app_sum_accum_comm_block_sec_;

  yaml_out << YAML::Key << "app_sum_accum_comm_block_sec_percent"
	   << YAML::Value << double(app_sum_accum_comm_block_sec_)
    / double(app_accum_comp_sec_);

  yaml_out << YAML::Key << "app_sum_approx_ssp_get_hit_sec"
	   << YAML::Value << app_sum_approx_ssp_get_hit_sec_;

  yaml_out << YAML::Key << "app_sum_approx_ssp_get_hit_sec_percent"
	   << YAML::Value << double(app_sum_approx_ssp_get_hit_sec_)
    / double(app_accum_comp_sec_);

  yaml_out << YAML::Key << "app_sum_approx_inc_sec"
	   << YAML::Value << app_sum_approx_inc_sec_;

  yaml_out << YAML::Key << "app_sum_approx_inc_sec_percent"
	   << YAML::Value << double(app_sum_approx_inc_sec_)
    / double(app_accum_comp_sec_);

  yaml_out << YAML::Key << "app_sum_approx_batch_inc_sec"
	   << YAML::Value << app_sum_approx_batch_inc_sec_;

  yaml_out << YAML::Key << "app_sum_approx_batch_inc_sec_percent"
	   << YAML::Value << double(app_sum_approx_batch_inc_sec_)
    / double(app_accum_comp_sec_);

  yaml_out << YAML::Key << "app_accum_tg_clock_sec"
	   << YAML::Value << app_accum_tg_clock_sec_;

  yaml_out << YAML::Key << "app_accum_tg_clock_sec_percent"
	   << YAML::Value << double(app_accum_tg_clock_sec_)
    / double(app_accum_comp_sec_);

  yaml_out << YAML::Key << "ps_overall_overhead"
	   << YAML::Value
	   << (double(app_sum_accum_comm_block_sec_
		      + app_sum_approx_ssp_get_hit_sec_
		      + app_sum_approx_inc_sec_
		      + app_sum_approx_batch_inc_sec_
		      + app_accum_tg_clock_sec_)
	       / double(app_accum_comp_sec_));

  yaml_out << YAML::EndMap;

  for (auto table_stats_iter = table_stats_.begin();
       table_stats_iter != table_stats_.end(); table_stats_iter++) {
    std::stringstream ss;
    ss << "Table." << table_stats_iter->first;
    yaml_out << YAML::BeginMap
             << YAML::Comment(ss.str().c_str())
             << YAML::Key << "num_get"
             << YAML::Value << table_stats_iter->second.num_get
             << YAML::Key << "num_ssp_get_hit"
             << YAML::Value << table_stats_iter->second.num_ssp_get_hit
             << YAML::Key << "num_ssp_get_miss"
             << YAML::Value << table_stats_iter->second.num_ssp_get_miss
	     << YAML::Key << "num_ssp_get_hit_sampled"
	     << YAML::Value << table_stats_iter->second.num_ssp_get_hit_sampled
	     << YAML::Key << "num_ssp_get_miss_sampled"
	     << YAML::Value << table_stats_iter->second.num_ssp_get_miss_sampled
             << YAML::Key << "accum_sample_ssp_get_hit_sec"
             << YAML::Value
             << table_stats_iter->second.accum_sample_ssp_get_hit_sec
             << YAML::Key << "accum_sample_ssp_get_miss_sec"
             << YAML::Value
             << table_stats_iter->second.accum_sample_ssp_get_miss_sec
	     << YAML::Key << "num_ssppush_get_comm_block"
	     << YAML::Value << table_stats_iter->second.num_ssppush_get_comm_block
	     << YAML::Key << "accum_sspppush_get_comm_block_sec"
	     << YAML::Value << table_stats_iter->second.accum_ssppush_get_comm_block_sec
	     << YAML::Key << "accum_ssp_get_server_fetch_sec"
	     << YAML::Value << table_stats_iter->second.accum_ssp_get_server_fetch_sec
             << YAML::Key << "num_inc"
             << YAML::Value << table_stats_iter->second.num_inc
	     << YAML::Key << "num_inc_sampled"
	     << YAML::Value << table_stats_iter->second.num_inc_sampled
             << YAML::Key << "accum_sample_inc_sec"
             << YAML::Value << table_stats_iter->second.accum_sample_inc_sec
             << YAML::Key << "num_batch_inc"
	     << YAML::Value << table_stats_iter->second.num_batch_inc
	     << YAML::Key << "num_batch_inc_sampled"
	     << YAML::Value << table_stats_iter->second.num_batch_inc_sampled
	     << YAML::Key << "accum_sample_batch_inc_sec"
             << YAML::Value
             << table_stats_iter->second.accum_sample_batch_inc_sec
	     << YAML::Key << "num_clock"
	     << YAML::Value << table_stats_iter->second.num_clock
	     << YAML::Key << "accum_sample_clock_sec"
	     << YAML::Value << table_stats_iter->second.accum_sample_clock_sec
	     << YAML::Key << "num_thread_get"
	     << YAML::Value << table_stats_iter->second.num_thread_get
	     << YAML::Key << "accum_sample_thread_get_sec"
	     << YAML::Value << table_stats_iter->second.accum_sample_thread_get_sec
             << YAML::EndMap;
  }

  yaml_out << YAML::BeginMap
           << YAML::Comment("BgThread Stats")
	   << YAML::Key << "bg_accum_clock_end_oplog_serialize_sec"
	   << YAML::Value << bg_accum_clock_end_oplog_serialize_sec_
	   << YAML::Key << "bg_accum_total_oplog_serialize_sec"
	   << YAML::Value << bg_accum_total_oplog_serialize_sec_
	   << YAML::Key << "bg_accum_server_push_row_apply_sec"
	   << YAML::Value << bg_accum_server_push_row_apply_sec_
	   << YAML::Key << "bg_accum_oplog_sent_mb"
	   << YAML::Value << bg_accum_oplog_sent_mb_
	   << YAML::Key << "bg_accum_server_push_row_recv_mb"
	   << YAML::Value << bg_accum_server_push_row_recv_mb_;

  yaml_out << YAML::Key << "bg_per_clock_oplog_sent_mb"
	   << YAML::Value;
  YamlPrintSequence(&yaml_out, bg_per_clock_oplog_sent_mb_);

  yaml_out << YAML::Key << "bg_per_clock_server_push_row_recv_mb"
	   << YAML::Value;
  YamlPrintSequence(&yaml_out, bg_per_clock_server_push_row_recv_mb_);

  yaml_out << YAML::EndMap;

  yaml_out << YAML::BeginMap
           << YAML::Comment("ServerThread Stats")
	   << YAML::Key << "server_accum_apply_oplog_sec"
	   << YAML::Value << server_accum_apply_oplog_sec_
	   << YAML::Key << "server_accum_push_row_sec"
	   << YAML::Value << server_accum_push_row_sec_
	   << YAML::Key << "server_accum_oplog_recv_mb"
	   << YAML::Value << server_accum_oplog_recv_mb_
	   << YAML::Key << "server_accum_push_row_mb"
	   << YAML::Value << server_accum_push_row_mb_;

  yaml_out << YAML::Key << "server_per_clock_oplog_recv_mb"
	   << YAML::Value;
  YamlPrintSequence(&yaml_out, server_per_clock_oplog_recv_mb_);

  yaml_out << YAML::Key << "server_per_clock_push_row_mb"
	   << YAML::Value;
  YamlPrintSequence(&yaml_out, server_per_clock_push_row_mb_);

  yaml_out << YAML::EndMap;

  std::fstream of_stream(stats_path_, std::ios_base::out
                         | std::ios_base::trunc);

  of_stream << yaml_out.c_str();

  of_stream.close();

  std::stringstream app_defined_vec_ss;
  app_defined_vec_ss << stats_path_ << ".app-def-" << app_defined_vec_name_;

  std::fstream app_defined_vec_of(app_defined_vec_ss.str(), std::ios_base::out
				  | std::ios_base::trunc);

  int i = 1;
  for (auto vec_iter = app_defined_vec_.begin(); vec_iter != app_defined_vec_.end();
       ++vec_iter) {
    app_defined_vec_of << i << "\t" << *vec_iter << "\n";
    ++i;
  }

  app_defined_vec_of.close();
}

}
