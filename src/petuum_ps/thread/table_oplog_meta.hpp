#pragma once

#include <petuum_ps/oplog/row_oplog_meta.hpp>
#include <petuum_ps_common/include/abstract_row.hpp>
#include <petuum_ps_common/include/configs.hpp>

#include <boost/unordered_map.hpp>
#include <list>
#include <stdint.h>
#include <petuum_ps/thread/context.hpp>
#include <boost/noncopyable.hpp>

namespace petuum {

class TableOpLogMeta : boost::noncopyable {
public:
  TableOpLogMeta(const AbstractRow *sample_row);
  ~TableOpLogMeta();

  TableOpLogMeta(TableOpLogMeta && other) {
    oplog_map_ = other.oplog_map_;
    oplog_list_ = other.oplog_list_;
    other.oplog_map_.clear();
    other.oplog_list_.clear();

    CompRowOpLogMeta_ = other.CompRowOpLogMeta_;
    ReassignImportance_ = other.ReassignImportance_;
    MergeRowOpLogMeta_ = other.MergeRowOpLogMeta_;
  }

  typedef bool (*CompRowOpLogMetaFunc)(
      const std::pair<int32_t, RowOpLogMeta*> &oplog1,
      const std::pair<int32_t, RowOpLogMeta*> &oplog2);

  typedef void (*ReassignImportanceFunc)(
      std::list<std::pair<int32_t, RowOpLogMeta*> > *oplog_list);

  typedef void (*MergeRowOpLogMetaFunc)(RowOpLogMeta* row_oplog_meta,
                                        const RowOpLogMeta& to_merge);

  void InsertMergeRowOpLogMeta(int32_t row_id,
                               const RowOpLogMeta& row_oplog_meta);

  void Sort();
  // Assuming sort has happened
  int32_t GetAndClearNextInOrder();
  int32_t GetAndClearNextInOrder(double *importance);

  int32_t InitGetUptoClock(int32_t clock);
  int32_t GetAndClearNextUptoClock();

  size_t GetNumRowOpLogs() const {
    return oplog_map_.size();
  }

private:
  static bool CompRowOpLogMetaClock(
      const std::pair<int32_t, RowOpLogMeta*> &oplog1,
      const std::pair<int32_t, RowOpLogMeta*> &oplog2);

  static bool CompRowOpLogMetaImportance(
      const std::pair<int32_t, RowOpLogMeta*> &oplog1,
      const std::pair<int32_t, RowOpLogMeta*> &oplog2);

  static bool CompRowOpLogMetaRelativeFIFONReMag(
      const std::pair<int32_t, RowOpLogMeta*> &oplog1,
      const std::pair<int32_t, RowOpLogMeta*> &oplog2);

  static void ReassignImportanceRandom(
      std::list<std::pair<int32_t, RowOpLogMeta*> > *oplog_list);

  static void ReassignImportanceNoOp(
      std::list<std::pair<int32_t, RowOpLogMeta*> > *oplog_list);

  static void MergeRowOpLogMetaAccum(RowOpLogMeta* row_oplog_meta,
                                     const RowOpLogMeta& to_merge);

  // return importance1
  static void MergeRowOpLogMetaNoOp(RowOpLogMeta* row_oplog_meta,
                                    const RowOpLogMeta& to_merge);

  boost::unordered_map<int32_t, RowOpLogMeta*> oplog_map_;
  std::list<std::pair<int32_t, RowOpLogMeta*> > oplog_list_;

  const AbstractRow *sample_row_;
  MergeRowOpLogMetaFunc MergeRowOpLogMeta_;
  CompRowOpLogMetaFunc CompRowOpLogMeta_;
  ReassignImportanceFunc ReassignImportance_;

  std::list<std::pair<int32_t, RowOpLogMeta*> >::iterator list_iter_;

  // After GetAndClearNextUptoClock(), all row oplogs will be above this clock
  // (exclusive)
  // Assume oplogs below this clock will not be added after GetAndClear...
  int32_t clock_to_clear_;
};

}
