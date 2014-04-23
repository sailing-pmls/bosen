// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.04.07

#pragma once

#ifndef NDEBUG

#define REGISTER_THREAD_FOR_STATS(add_to_total_time) \
  Stats::RegisterThread(add_to_total_time)

#define TIMER_BEGIN(table_id, type) \
  Stats::TimerBegin(table_id, type)

#define TIMER_END(table_id, type) \
  Stats::TimerEnd(table_id, type)

#define FINALIZE_STATS() \
  Stats::FinalizeStats()

#define PRINT_STATS() \
  Stats::PrintStats()

#else  // NDEBUG

#define REGISTER_THREAD_FOR_STATS(add_to_total_time) ((void) 0)
#define TIMER_BEGIN(table_id, type) ((void) 0)
#define TIMER_END(table_id, type) ((void) 0)
#define FINALIZE_STATS() ((void) 0)
#define PRINT_STATS() ((void) 0)

#endif  // NDEBUG


#include "petuum_ps/util/high_resolution_timer.hpp"
#include <boost/thread/tss.hpp>
#include <cstdint>
#include <string>
#include <mutex>


namespace petuum {

// Adding new StatsType requires change in both enum and
// kStatsTypeName.
enum StatsType {
  NO_STATS = 0,  // track no stats.
  INC = 1,
  BATCH_INC = 2,
  GET = 3,
  CLOCK = 4,
  SSP_PSTORAGE_FIND = 5,
  SSP_ROW_REQUEST = 6,
  BG_CREATE_SEND_OPLOG = 7,
  BG_CREATE_OPLOG_PREP = 8,
  BG_CREATE_OPLOG_MKMEM = 9,
  BG_CREATE_OPLOG_SERIALIZE = 10,
  SERVER_HANDLE_OPLOG_MSG = 11,
  SORTED_VECTOR_MAP_BATCH_INC_UNSAFE = 12,
  SSPPUSH_BATCH_INC_THR_UPDATE_INDEX = 13,
  SSPPUSH_BATCH_INC_OPLOG = 14,
  SSPPUSH_BATCH_INC_PROCESS_STORAGE = 15,
  SSPPUSH_THREAD_BATCH_INC = 16
};

const std::vector<std::string> kStatsTypeName =
  {"NO_STATS", "INC", "BATCH_INC", "GET", "CLOCK", "SSP_PSTORAGE_FIND", "SSP_ROW_REQUEST",
   "BG_CREATE_SEND_OPLOG", "BG_CREATE_OPLOG_PREP", "BG_CREATE_OPLOG_MKMEM",
   "BG_CREATE_OPLOG_SERIALIZE", "SERVER_HANDLE_OPLOG_MSG", "SORTED_VECTOR_MAP_BATCH_INC_UNSAFE",
   "SSPPUSH_BATCH_INC_THR_UPDATE_INDEX", "SSPPUSH_BATCH_INC_OPLOG", "SSPPUSH_BATCH_INC_PRCESS_STORAGE",
   "SSPPUSH_THREAD_BATCH_INC"};


class Stats {
public:
  static void RegisterThread(bool add_to_total_time);

  static void TimerBegin(int32_t table_id, StatsType type);

  static void TimerEnd(int32_t table_id, StatsType type);

  // Sum all thread data to shared data fields.
  static void FinalizeStats();

  static void PrintStats();

private:
  // Check whether <table_id, type> is on our radar.
  static bool CheckSelected(int32_t table_id, StatsType type);

  struct StatsThreadData {
    // <table_id, type> is mapped to index
    // (table_id * FLAGS_max_num_tables) + type.
    std::vector<int32_t> call_counts;
    std::vector<double> times;

    std::vector<HighResolutionTimer> timers;
    HighResolutionTimer total_time;
    bool add_to_total_time;

    explicit StatsThreadData(int num_items, bool _add_to_total_time) :
      call_counts(num_items), times(num_items), timers(num_items),
      add_to_total_time(_add_to_total_time) { }
  };

  static int32_t num_items_;
  static int32_t num_stats_types_;

  // Sum of thread-level stats.
  static std::vector<int32_t> call_counts_;
  static std::vector<double> times_;

  // Sum of thread-duration from RegisterThread() to FinalizeStats().
  static double total_time_;
  static std::mutex mtx_;

  // See max_num_tables flag.
  static int32_t max_num_tables_;

  static boost::thread_specific_ptr<StatsThreadData> thread_data_;
};

}   // namespace petuum
