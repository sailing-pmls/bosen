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
double Stats::app_sum_approx_batch_inc_oplog_sec_ = 0.;
double Stats::app_sum_approx_batch_inc_process_storage_sec_ = 0.;

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

std::vector<double> Stats::app_accum_append_only_flush_oplog_sec_;
std::vector<size_t> Stats::app_append_only_flush_oplog_count_;

double Stats::bg_accum_clock_end_oplog_serialize_sec_ = 0;
double Stats::bg_accum_total_oplog_serialize_sec_ = 0;
double Stats::bg_accum_server_push_row_apply_sec_ = 0;
double Stats::bg_accum_oplog_sent_mb_ = 0;
double Stats::bg_accum_server_push_row_recv_mb_ = 0;

std::vector<double> Stats::bg_per_clock_oplog_sent_mb_;
std::vector<double> Stats::bg_per_clock_server_push_row_recv_mb_;

std::vector<size_t> Stats::bg_accum_server_push_oplog_row_applied_;
std::vector<size_t> Stats::bg_accum_server_push_update_applied_;
std::vector<size_t> Stats::bg_accum_server_push_version_diff_;

std::vector<double> Stats::bg_sample_process_cache_insert_sec_;
std::vector<size_t> Stats::bg_num_process_cache_insert_;
std::vector<size_t> Stats::bg_num_process_cache_insert_sampled_;

std::vector<double> Stats::bg_sample_server_push_deserialize_sec_;
std::vector<size_t> Stats::bg_num_server_push_deserialize_;
std::vector<size_t> Stats::bg_num_server_push_deserialize_sampled_;

std::vector<size_t> Stats::bg_accum_num_idle_invoke_;
std::vector<size_t> Stats::bg_accum_num_idle_send_;
std::vector<size_t> Stats::bg_accum_num_push_row_msg_recv_;
std::vector<double> Stats::bg_accum_idle_send_sec_;
std::vector<size_t> Stats::bg_accum_idle_send_bytes_;

std::vector<double> Stats::bg_accum_handle_append_oplog_sec_;
std::vector<size_t> Stats::bg_num_append_oplog_buff_handled_;

std::vector<size_t> Stats::bg_num_row_oplog_created_;
std::vector<size_t> Stats::bg_num_row_oplog_recycled_;

double Stats::server_accum_apply_oplog_sec_ = 0.0;
double Stats::server_accum_push_row_sec_ = 0.0;

double Stats::server_accum_oplog_recv_mb_ = 0.0;
double Stats::server_accum_push_row_mb_ = 0.0;

std::vector<double> Stats::server_per_clock_oplog_recv_mb_;
std::vector<double> Stats::server_per_clock_push_row_mb_;

std::vector<size_t> Stats::server_accum_num_oplog_msg_recv_;
std::vector<size_t> Stats::server_accum_num_push_row_msg_send_;

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

  app_defined_accum_sec_vec_.push_back(
      app_thread_stats_->app_defined_accum_sec);

  app_defined_accum_val_ += app_thread_stats_->app_defined_accum_val;

  app_accum_comp_sec_ += app_thread_stats_->accum_comp_sec;
  app_accum_obj_comp_sec_ += app_thread_stats_->accum_obj_comp_sec;
  app_accum_tg_clock_sec_ += app_thread_stats_->accum_tg_clock_sec;

  // Detailed BatchInc stats.
  if (app_thread_stats_->num_batch_inc_oplog_sampled != 0) {
    app_sum_approx_batch_inc_oplog_sec_ +=
      double(app_thread_stats_->num_batch_inc_oplog)
      / double(app_thread_stats_->num_batch_inc_oplog_sampled)
      * app_thread_stats_->accum_sample_batch_inc_oplog_sec;
  }

  if (app_thread_stats_->num_batch_inc_process_storage_sampled != 0) {
    app_sum_approx_batch_inc_process_storage_sec_ +=
      double(app_thread_stats_->num_batch_inc_process_storage)
      / double(app_thread_stats_->num_batch_inc_process_storage_sampled)
      * app_thread_stats_->accum_sample_batch_inc_process_storage_sec;
  }

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

  app_accum_append_only_flush_oplog_sec_.push_back(
      app_thread_stats_->accum_append_only_oplog_flush_sec);

  app_append_only_flush_oplog_count_.push_back(
      app_thread_stats_->append_only_flush_oplog_count);
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
    += stats.accum_oplog_sent_kb / double(k1_Ki);

  bg_accum_server_push_row_recv_mb_
    += stats.accum_server_push_row_recv_kb / double(k1_Ki);

  bg_accum_server_push_oplog_row_applied_.push_back(
      stats.accum_server_push_oplog_row_applied);

  bg_accum_server_push_update_applied_.push_back(
      stats.accum_server_push_update_applied);

  bg_accum_server_push_version_diff_.push_back(
      stats.accum_server_push_version_diff);

  bg_sample_process_cache_insert_sec_.push_back(
      stats.sample_process_cache_insert_sec);

  bg_num_process_cache_insert_.push_back(
      stats.num_process_cache_insert);

  bg_num_process_cache_insert_sampled_.push_back(
      stats.num_process_cache_insert_sampled);

  bg_sample_server_push_deserialize_sec_.push_back(
      stats.sample_server_push_deserialize_sec);

  bg_num_server_push_deserialize_.push_back(
      stats.num_server_push_deserialize);

  bg_num_server_push_deserialize_sampled_.push_back(
      stats.num_server_push_deserialize_sampled);

  size_t vec_size = stats.per_clock_oplog_sent_kb.size();

  if (bg_per_clock_oplog_sent_mb_.size() < vec_size) {
    bg_per_clock_oplog_sent_mb_.resize(vec_size, 0.0);
  }

  for (int i = 0; i < vec_size; ++i) {
    bg_per_clock_oplog_sent_mb_[i]
      += stats.per_clock_oplog_sent_kb[i] / double(k1_Ki);
  }

  vec_size = stats.per_clock_server_push_row_recv_kb.size();

  if (bg_per_clock_server_push_row_recv_mb_.size() < vec_size) {
    bg_per_clock_server_push_row_recv_mb_.resize(vec_size, 0.0);
  }

  for (int i = 0; i < vec_size; ++i) {
    bg_per_clock_server_push_row_recv_mb_[i]
      += stats.per_clock_server_push_row_recv_kb[i] / double(k1_Ki);
  }

  bg_accum_num_idle_invoke_.push_back(stats.accum_num_idle_invoke);
  bg_accum_num_idle_send_.push_back(stats.accum_num_idle_send);
  bg_accum_num_push_row_msg_recv_.push_back(stats.accum_num_push_row_msg_recv);
  bg_accum_idle_send_sec_.push_back(stats.accum_idle_send_sec);
  bg_accum_idle_send_bytes_.push_back(stats.accum_idle_send_bytes);

  bg_accum_handle_append_oplog_sec_.push_back(stats.accum_handle_append_oplog_sec);
  bg_num_append_oplog_buff_handled_.push_back(stats.num_append_oplog_buff_handled);

  bg_num_row_oplog_created_.push_back(stats.num_row_oplog_created);
  bg_num_row_oplog_recycled_.push_back(stats.num_row_oplog_recycled);
}

void Stats::DeregisterServerThread() {
  std::lock_guard<std::mutex> lock(stats_mtx_);
  ServerThreadStats &stats = *server_thread_stats_;
  server_accum_apply_oplog_sec_
    += stats.accum_apply_oplog_sec;

  server_accum_push_row_sec_
    += stats.accum_push_row_sec;

  server_accum_oplog_recv_mb_
    += stats.accum_oplog_recv_kb / double(k1_Ki);

  server_accum_push_row_mb_
    += stats.accum_push_row_kb / double(k1_Ki);

  size_t vec_size = stats.per_clock_oplog_recv_kb.size();

  if (server_per_clock_oplog_recv_mb_.size() < vec_size) {
    server_per_clock_oplog_recv_mb_.resize(vec_size, 0.0);
  }

  for (int i = 0; i < vec_size; ++i) {
    server_per_clock_oplog_recv_mb_[i]
      += stats.per_clock_oplog_recv_kb[i] / double(k1_Ki);
  }

  vec_size = stats.per_clock_push_row_kb.size();

  if (server_per_clock_push_row_mb_.size() < vec_size) {
    server_per_clock_push_row_mb_.resize(vec_size, 0.0);
  }

  for (int i = 0; i < vec_size; ++i) {
    server_per_clock_push_row_mb_[i]
      += stats.per_clock_push_row_kb[i] / double(k1_Ki);
  }

  server_accum_num_oplog_msg_recv_.push_back(stats.accum_num_oplog_msg_recv);
  server_accum_num_push_row_msg_send_.push_back(
      stats.accum_num_push_row_msg_send);
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

void Stats::AppAccumTgClockBegin() {
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
        += stats.table_stats[table_id].get_timer.elapsed();
    ++stats.table_stats[table_id].num_ssp_get_hit_sampled;
  } else {
    stats.table_stats[table_id].accum_sample_ssp_get_miss_sec
        += stats.table_stats[table_id].get_timer.elapsed();
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

void Stats::AppSampleBatchIncOplogBegin() {
  AppThreadStats &stats = *app_thread_stats_;

  if (stats.num_batch_inc_oplog % kBatchIncOplogSampleFreq)
    return;

  stats.batch_inc_oplog_timer.restart();
}

void Stats::AppSampleBatchIncOplogEnd() {
  AppThreadStats &stats = *app_thread_stats_;

  uint64_t org_num_batch_inc_oplog =
    stats.num_batch_inc_oplog;
  ++stats.num_batch_inc_oplog;
  if (org_num_batch_inc_oplog % kBatchIncOplogSampleFreq)
    return;

  stats.accum_sample_batch_inc_oplog_sec
    += stats.batch_inc_oplog_timer.elapsed();
  ++stats.num_batch_inc_oplog_sampled;
}

void Stats::AppSampleBatchIncProcessStorageBegin() {
  AppThreadStats &stats = *app_thread_stats_;

  if (stats.num_batch_inc_process_storage %
      kBatchIncProcessStorageSampleFreq)
    return;

  stats.batch_inc_process_storage_timer.restart();
}

void Stats::AppSampleBatchIncProcessStorageEnd() {
  AppThreadStats &stats = *app_thread_stats_;

  uint64_t org_num_batch_inc_process_storage =
    stats.num_batch_inc_process_storage;
  ++stats.num_batch_inc_process_storage;
  if (org_num_batch_inc_process_storage % kBatchIncProcessStorageSampleFreq)
    return;

  stats.accum_sample_batch_inc_process_storage_sec
    += stats.batch_inc_process_storage_timer.elapsed();
  ++stats.num_batch_inc_process_storage_sampled;
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

void Stats::AppAccumAppendOnlyFlushOpLogBegin() {
  app_thread_stats_->append_only_oplog_flush_timer.restart();
}

void Stats::AppAccumAppendOnlyFlushOpLogEnd() {
  AppThreadStats &stats = *app_thread_stats_;

  stats.accum_append_only_oplog_flush_sec
      += stats.append_only_oplog_flush_timer.elapsed();

  ++(stats.append_only_flush_oplog_count);
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

void Stats::BgSampleProcessCacheInsertBegin() {
  BgThreadStats &stats = *bg_thread_stats_;

  if (stats.num_process_cache_insert % kProcessCacheInsertSampleFreq)
    return;

  stats.process_cache_insert_timer.restart();
}

void Stats::BgSampleProcessCacheInsertEnd() {
  BgThreadStats &stats = *bg_thread_stats_;

  uint64_t org_num_process_cache_insert
    = stats.num_process_cache_insert;
  ++(stats.num_process_cache_insert);
  if (org_num_process_cache_insert % kProcessCacheInsertSampleFreq)
    return;

  stats.sample_process_cache_insert_sec
    += stats.process_cache_insert_timer.elapsed();
  ++stats.num_process_cache_insert_sampled;
}

void Stats::BgSampleServerPushDeserializeBegin() {
  BgThreadStats &stats = *bg_thread_stats_;

  if (stats.num_server_push_deserialize % kServerPushDeserializeSampleFreq)
    return;

  stats.server_push_deserialize_timer.restart();
}

void Stats::BgSampleServerPushDeserializeEnd() {
  BgThreadStats &stats = *bg_thread_stats_;

  uint64_t org_num_server_push_deserialize
    = stats.num_server_push_deserialize;
  ++(stats.num_server_push_deserialize);
  if (org_num_server_push_deserialize % kServerPushDeserializeSampleFreq)
    return;

  stats.sample_server_push_deserialize_sec
    += stats.server_push_deserialize_timer.elapsed();
  ++stats.num_server_push_deserialize_sampled;
}

void Stats::BgClock() {
  ++(bg_thread_stats_->clock_num);
  bg_thread_stats_->per_clock_oplog_sent_kb.push_back(0.0);
  bg_thread_stats_->per_clock_server_push_row_recv_kb.push_back(0.0);
}

void Stats::BgAddPerClockOpLogSize(size_t oplog_size) {
  double oplog_size_kb
    = double(oplog_size) / double(k1_Ki);

  BgThreadStats &stats = *bg_thread_stats_;

  stats.per_clock_oplog_sent_kb[stats.clock_num]
    += oplog_size_kb;
  stats.accum_oplog_sent_kb += oplog_size_kb;
}

void Stats::BgAddPerClockServerPushRowSize(size_t server_push_row_size) {
  double server_push_row_size_kb
    = double(server_push_row_size) / double(k1_Ki);

  BgThreadStats &stats = *bg_thread_stats_;

  stats.per_clock_server_push_row_recv_kb[stats.clock_num]
    += server_push_row_size_kb;
  stats.accum_server_push_row_recv_kb += server_push_row_size_kb;
}

void Stats::BgIdleInvokeIncOne() {
  ++(bg_thread_stats_->accum_num_idle_invoke);
}

void Stats::BgIdleSendIncOne() {
  ++(bg_thread_stats_->accum_num_idle_send);
}

void Stats::BgAccumPushRowMsgReceivedIncOne() {
  ++(bg_thread_stats_->accum_num_push_row_msg_recv);
}

void Stats::BgAccumIdleSendBegin() {
  bg_thread_stats_->idle_send_timer.restart();
}

void Stats::BgAccumIdleSendEnd() {
  BgThreadStats &stats = *bg_thread_stats_;
  stats.accum_idle_send_sec += stats.idle_send_timer.elapsed();
  stats.accum_total_oplog_serialize_sec += stats.idle_send_timer.elapsed();
}

void Stats::BgAccumIdleOpLogSentBytes(size_t num_bytes) {
  bg_thread_stats_->accum_idle_send_bytes += num_bytes;
}

void Stats::BgAccumHandleAppendOpLogBegin() {
  bg_thread_stats_->handle_append_oplog_timer.restart();
}

void Stats::BgAccumHandleAppendOpLogEnd() {
  BgThreadStats &stats = *bg_thread_stats_;
  stats.accum_handle_append_oplog_sec += stats.handle_append_oplog_timer.elapsed();
  ++(stats.num_append_oplog_buff_handled);
}

void Stats::BgAppendOnlyCreateRowOpLogInc() {
  ++(bg_thread_stats_->num_row_oplog_created);
}

void Stats::BgAppendOnlyRecycleRowOpLogInc() {
  ++(bg_thread_stats_->num_row_oplog_recycled);
}

void Stats::BgAccumServerPushOpLogRowAppliedAddOne() {
  ++(bg_thread_stats_->accum_server_push_oplog_row_applied);
}

void Stats::BgAccumServerPushUpdateAppliedAddOne() {
  ++(bg_thread_stats_->accum_server_push_update_applied);
}

void Stats::BgAccumServerPushVersionDiffAdd(size_t diff) {
  bg_thread_stats_->accum_server_push_version_diff += diff;
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
    = double(oplog_size) / double(k1_Ki);

  ServerThreadStats &stats = *server_thread_stats_;

  stats.per_clock_oplog_recv_kb[stats.clock_num]
    += oplog_size_kb;
  stats.accum_oplog_recv_kb += oplog_size_kb;
}

void Stats::ServerAddPerClockPushRowSize(size_t push_row_size) {
  double push_row_size_kb
    = double(push_row_size) / double(k1_Ki);

  ServerThreadStats &stats = *server_thread_stats_;

  stats.per_clock_push_row_kb[stats.clock_num]
    += push_row_size_kb;
  stats.accum_push_row_kb += push_row_size_kb;
}

void Stats::ServerOpLogMsgRecvIncOne() {
  ++(server_thread_stats_->accum_num_oplog_msg_recv);
}

void Stats::ServerPushRowMsgSendIncOne() {
  ++(server_thread_stats_->accum_num_push_row_msg_send);
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
    << YAML::Key << "num_comm_channels_per_client"
    << YAML::Value << table_group_config_.num_comm_channels_per_client
    << YAML::Key << "num_tables"
    << YAML::Value << table_group_config_.num_tables
    << YAML::Key << "num_total_clients"
    << YAML::Value << table_group_config_.num_total_clients
    << YAML::Key << "num_local_app_threads"
    << YAML::Value << table_group_config_.num_local_app_threads
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

  yaml_out << YAML::Key << "app_sum_approx_batch_inc_oplog_sec"
    << YAML::Value << app_sum_approx_batch_inc_oplog_sec_;

  yaml_out << YAML::Key << "app_sum_approx_batch_inc_oplog_sec_percent"
    << YAML::Value << double(app_sum_approx_batch_inc_oplog_sec_)
    / double(app_accum_comp_sec_);

  yaml_out << YAML::Key << "app_sum_approx_batch_inc_process_storage_sec"
    << YAML::Value << app_sum_approx_batch_inc_process_storage_sec_;

  yaml_out << YAML::Key
    << "app_sum_approx_batch_inc_process_storage_sec_percent"
    << YAML::Value << double(app_sum_approx_batch_inc_process_storage_sec_)
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
          // Don't sum over app_sum_approx_batch_inc_oplog_sec_
          // and app_sum_approx_batch_inc_process_sec_ to avoid double counting
          + app_accum_tg_clock_sec_)
        / double(app_accum_comp_sec_));

  yaml_out << YAML::Key << "app_accum_append_only_flush_oplog_sec"
           << YAML::Value;
  YamlPrintSequence(&yaml_out, app_accum_append_only_flush_oplog_sec_);

  yaml_out << YAML::Key << "app_accum_append_only_flush_oplog_count"
           << YAML::Value;
  YamlPrintSequence(&yaml_out, app_append_only_flush_oplog_count_);

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
      << YAML::Value
      << table_stats_iter->second.accum_ssppush_get_comm_block_sec
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

  yaml_out << YAML::Key << "bg_accum_server_push_oplog_row_applied"
    << YAML::Value;

  YamlPrintSequence(&yaml_out, bg_accum_server_push_oplog_row_applied_);

  yaml_out << YAML::Key << "bg_accum_server_push_update_applied"
    << YAML::Value;
  YamlPrintSequence(&yaml_out, bg_accum_server_push_update_applied_);

  yaml_out << YAML::Key << "bg_accum_server_push_version_diff"
    << YAML::Value;
  YamlPrintSequence(&yaml_out, bg_accum_server_push_version_diff_);

  yaml_out << YAML::Key << "bg_sample_process_cache_insert_sec"
    << YAML::Value;
  YamlPrintSequence(&yaml_out, bg_sample_process_cache_insert_sec_);
  yaml_out << YAML::Key << "bg_num_process_cache_insert"
    << YAML::Value;
  YamlPrintSequence(&yaml_out, bg_num_process_cache_insert_);
  yaml_out << YAML::Key << "bg_num_process_cache_insert_sampled"
    << YAML::Value;
  YamlPrintSequence(&yaml_out, bg_num_process_cache_insert_sampled_);

  yaml_out << YAML::Key << "bg_sample_server_push_deserialize_sec"
    << YAML::Value;
  YamlPrintSequence(&yaml_out, bg_sample_server_push_deserialize_sec_);

  yaml_out << YAML::Key << "bg_num_server_push_deserialize"
    << YAML::Value;
  YamlPrintSequence(&yaml_out, bg_num_server_push_deserialize_);

  yaml_out << YAML::Key << "bg_num_server_push_deserialize_sampled"
    << YAML::Value;
  YamlPrintSequence(&yaml_out, bg_num_server_push_deserialize_sampled_);

  yaml_out << YAML::Key << "bg_accum_num_idle_invoke"
    << YAML::Value;
  YamlPrintSequence(&yaml_out, bg_accum_num_idle_invoke_);

  yaml_out << YAML::Key << "bg_accum_num_idle_send"
    << YAML::Value;
  YamlPrintSequence(&yaml_out, bg_accum_num_idle_send_);

  yaml_out << YAML::Key << "bg_accum_num_push_row_msg_recv"
    << YAML::Value;
  YamlPrintSequence(&yaml_out, bg_accum_num_push_row_msg_recv_);

  yaml_out << YAML::Key << "bg_acccum_idle_send_sec"
    << YAML::Value;
  YamlPrintSequence(&yaml_out, bg_accum_idle_send_sec_);

  yaml_out << YAML::Key << "bg_acccum_idle_send_bytes"
    << YAML::Value;
  YamlPrintSequence(&yaml_out, bg_accum_idle_send_bytes_);

  yaml_out << YAML::Key << "bg_accum_handle_append_oplog_sec"
           << YAML::Value;
  YamlPrintSequence(&yaml_out, bg_accum_handle_append_oplog_sec_);

  yaml_out << YAML::Key << "bg_num_append_oplog_buff_handled"
           << YAML::Value;
  YamlPrintSequence(&yaml_out, bg_num_append_oplog_buff_handled_);

  yaml_out << YAML::Key << "bg_num_row_oplog_created"
           << YAML::Value;
  YamlPrintSequence(&yaml_out, bg_num_row_oplog_created_);

  yaml_out << YAML::Key << "bg_num_row_oplog_recycled"
           << YAML::Value;
  YamlPrintSequence(&yaml_out, bg_num_row_oplog_recycled_);

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

  yaml_out << YAML::Key << "server_accum_num_oplog_msg_recv"
    << YAML::Value;
  YamlPrintSequence(&yaml_out, server_accum_num_oplog_msg_recv_);

  yaml_out << YAML::Key << "server_accum_num_push_row_msg_send"
    << YAML::Value;
  YamlPrintSequence(&yaml_out, server_accum_num_push_row_msg_send_);

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
