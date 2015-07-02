#pragma once

#include <petuum_ps/thread/abstract_table_oplog_meta.hpp>

#include <vector>
#include <unordered_map>
#include <random>

namespace petuum {

struct IndexRowOpLogMeta {
  int32_t row_id;
  int32_t clock;
  double importance;
};

class ValueTableOpLogMeta : public AbstractTableOpLogMeta {
public:
  ValueTableOpLogMeta(const AbstractRow *sample_row, size_t table_size);
  virtual ~ValueTableOpLogMeta();

  void InsertMergeRowOpLogMeta(int32_t row_id,
                               const RowOpLogMeta& row_oplog_meta);
  virtual size_t GetCleanNumNewOpLogMeta();

  int32_t GetAndClearNextInOrder();

  int32_t InitGetUptoClock(int32_t clock);
  int32_t GetAndClearNextUptoClock();

  size_t GetNumRowOpLogs() const;

private:
  bool CompRowOpLogMeta(const IndexRowOpLogMeta &row_oplog_meta1,
                        const IndexRowOpLogMeta &row_oplog_meta2);
  int32_t HeapParent(int32_t index);
  int32_t HeapRight(int32_t index);
  int32_t HeapLeft(int32_t index);
  void HeapSwap(int32_t index1, int32_t index2);
  void HeapIncrease(int32_t index, const RowOpLogMeta &row_oplog_meta);
  void HeapInsert(int32_t row_id, const RowOpLogMeta &row_oplog_meta);
  IndexRowOpLogMeta HeapExtractMax();
  void HeapMaxHeapify(int32_t index);
  void HeapBuildMaxHeap();

  size_t table_size_;
  const AbstractRow *sample_row_;
  std::vector<IndexRowOpLogMeta> heap_;
  size_t heap_size_;
  std::vector<int32_t> heap_index_;
  int32_t heap_walker_;
  int32_t heap_last_;
  int32_t clock_to_clear_;

  size_t num_new_oplog_metas_;
};

}
