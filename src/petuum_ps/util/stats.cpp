// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.04.07

#include "petuum_ps/util/stats.hpp"
#include <sstream>
#include <glog/logging.h>
#include <gflags/gflags.h>

DEFINE_int32(PETUUM_stats_table_id, 0,
    "Choose a table to activate stats tracking. Default (0) no table "
    "but table-indepdent stats. -1 for all tables.");
DEFINE_int32(PETUUM_stats_type_id, 0,
    "Choose an stats type to activate stats tracking. Default (0) no type."
    " -1 for all types.");

DEFINE_int32(max_num_tables, 10,
    "Stats can keep track of [1, max_num_tables) tables. "
    "Table 0 keeps tracks of table-independent stats like Clock.");

namespace petuum {

int32_t Stats::max_num_tables_ = FLAGS_max_num_tables;
int32_t Stats::num_stats_types_ = kStatsTypeName.size();
int32_t Stats::num_items_ = FLAGS_max_num_tables * Stats::num_stats_types_;
std::vector<int32_t> Stats::call_counts_(Stats::num_items_);
std::vector<double> Stats::times_(Stats::num_items_);
double Stats::total_time_ = 0.;
std::mutex Stats::mtx_;
boost::thread_specific_ptr<Stats::StatsThreadData>
  Stats::thread_data_;

void Stats::RegisterThread(bool add_to_total_time) {
  thread_data_.reset(new StatsThreadData(num_items_, add_to_total_time));
}

void Stats::TimerBegin(int32_t table_id, StatsType type) {
  if (CheckSelected(table_id, type)) {
    int32_t idx = table_id * kStatsTypeName.size() + type;
    (thread_data_->timers)[idx].restart();
  }
}

void Stats::TimerEnd(int32_t table_id, StatsType type) {
  if (CheckSelected(table_id, type)) {
    int32_t idx = table_id * kStatsTypeName.size() + type;
    double elapse = (thread_data_->timers)[idx].elapsed();
    (thread_data_->times)[idx] += elapse;
    ++(thread_data_->call_counts)[idx];
  }
}

void Stats::FinalizeStats() {
  std::unique_lock<std::mutex> ulock(mtx_);
  for (int i = 0; i < num_items_; ++i) {
    call_counts_[i] += (thread_data_->call_counts)[i];
    times_[i] += (thread_data_->times)[i];
  }
  if (thread_data_->add_to_total_time)
    total_time_ += thread_data_->total_time.elapsed();
}

void Stats::PrintStats() {
  std::stringstream ss;
  std::unique_lock<std::mutex> ulock(mtx_);
  ss << "======= Total Duration (sum of all thread durations) "
    << total_time_ << " =========\n";
  for (int i = 0; i < num_items_; ++i) {
    int table_id = i / kStatsTypeName.size();
    StatsType type = static_cast<StatsType>(i % kStatsTypeName.size());
    //LOG(INFO) << "table_id = " << table_id << " type = " << type;
    if (CheckSelected(table_id, type)) {
      if (table_id != 0) {
        ss << "Table " << table_id << "\t"
          << kStatsTypeName[static_cast<int>(type)]
          << "\tcount: " << call_counts_[i] << "\ttime: " << times_[i]
          << " (" << times_[i] / total_time_ * 100. << "%)" << std::endl;
      } else {
        // Don't print Table id.
        ss << kStatsTypeName[static_cast<int>(type)]
          << "\tcount: " << call_counts_[i] << "\ttime: " << times_[i]
          << " (" << times_[i] / total_time_ * 100. << "%)" << std::endl;
      }
    }
  }
  ss << "==========================================================="
    << "===========";

  LOG(INFO) << "\n" << ss.str();
}

bool Stats::CheckSelected(int32_t table_id, StatsType type) {
  bool selected_table = (FLAGS_PETUUM_stats_table_id == -1)
    || (table_id == FLAGS_PETUUM_stats_table_id);
  bool selected_type = (FLAGS_PETUUM_stats_type_id == -1)
    || ((type != 0) && (type == FLAGS_PETUUM_stats_type_id));
  return selected_table && selected_type;
}

}  // namespace petuum
