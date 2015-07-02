#include <petuum_ps/thread/value_table_oplog_meta.hpp>

#include <petuum_ps/thread/context.hpp>
#include <algorithm>
namespace petuum {
ValueTableOpLogMeta::ValueTableOpLogMeta(const AbstractRow *sample_row,
                                         size_t table_size):
    table_size_(table_size),
    sample_row_(sample_row),
    heap_(table_size),
    heap_size_(0),
    heap_index_(table_size, -1),
    num_new_oplog_metas_(0) { }

ValueTableOpLogMeta::~ValueTableOpLogMeta() { }

bool ValueTableOpLogMeta::CompRowOpLogMeta(
    const IndexRowOpLogMeta &oplog1,
    const IndexRowOpLogMeta &oplog2) {

  if (oplog1.importance == oplog2.importance) {
    return oplog1.row_id < oplog2.row_id;
  } else {
    return (oplog1.importance > oplog2.importance);
    // min first
    //return (oplog1.importance <= oplog2.importance);
  }
}

void ValueTableOpLogMeta::HeapSwap(int32_t index1, int32_t index2) {
  int32_t row_id1 = heap_[index1].row_id;
  int32_t row_id2 = heap_[index2].row_id;

  std::swap(heap_[index1], heap_[index2]);
  std::swap(heap_index_[row_id1], heap_index_[row_id2]);
}

int32_t ValueTableOpLogMeta::HeapParent(int32_t index) {
  return (index - 1) / 2;
}

int32_t ValueTableOpLogMeta::HeapRight(int32_t index) {
  return index*2 + 2;
}

int32_t ValueTableOpLogMeta::HeapLeft(int32_t index) {
  return index*2 + 1;
}

void ValueTableOpLogMeta::HeapIncrease(
    int32_t index, const RowOpLogMeta &row_oplog_meta) {
  heap_[index].importance += row_oplog_meta.get_importance();
  if (heap_[index].clock == -1 || heap_[index].clock > row_oplog_meta.get_clock()) {
    heap_[index].clock = row_oplog_meta.get_clock();
  }

  int32_t check_index = index;
  while (check_index != 0) {
    int32_t parent_index = HeapParent(check_index);
    if (CompRowOpLogMeta(heap_[check_index], heap_[parent_index])) {
      HeapSwap(check_index, parent_index);
      check_index = parent_index;
    } else {
      break;
    }
  }
}

void ValueTableOpLogMeta::HeapInsert(
    int32_t row_id, const RowOpLogMeta &row_oplog_meta) {
  heap_size_++;
  int32_t heap_last = heap_size_ - 1;
  heap_[heap_last] = {row_id, -1, 0.0};
  heap_index_[row_id] = heap_last;

  HeapIncrease(heap_last, row_oplog_meta);
}

IndexRowOpLogMeta ValueTableOpLogMeta::HeapExtractMax() {
  if (heap_size_ == 0)
    return {-1, -1, 0.0};

  IndexRowOpLogMeta max = heap_.front();
  heap_size_--;
  heap_index_[max.row_id] = -1;

  if (heap_size_ > 0) {
    int32_t heap_last = heap_size_;
    heap_.front() = heap_[heap_last];
    heap_index_[heap_[heap_last].row_id] = 0;

    HeapMaxHeapify(0);
  }
  return max;
}

void ValueTableOpLogMeta::HeapMaxHeapify(int32_t index) {
  int32_t left = HeapLeft(index);
  int32_t right = HeapRight(index);

  int32_t largest = index;
  if (left < heap_size_ && CompRowOpLogMeta(heap_[left], heap_[index])) {
    largest = left;
  }

  if (right < heap_size_ && CompRowOpLogMeta(heap_[right], heap_[largest])) {
    largest = right;
  }

  if (largest != index) {
    HeapSwap(index, largest);
    HeapMaxHeapify(largest);
  }
}

void ValueTableOpLogMeta::HeapBuildMaxHeap() {
  if (heap_size_ == 0) return;
  int32_t sort_index = HeapParent((heap_size_ - 1));

  while (sort_index >= 0) {
    HeapMaxHeapify(sort_index);
    sort_index--;
  }
}

void ValueTableOpLogMeta::InsertMergeRowOpLogMeta(
    int32_t row_id,
    const RowOpLogMeta& row_oplog_meta) {
  int32_t index = heap_index_[row_id];
  if (index >= 0) {
    HeapIncrease(index, row_oplog_meta);
  } else {
    HeapInsert(row_id, row_oplog_meta);
    num_new_oplog_metas_++;
  }
}

size_t ValueTableOpLogMeta::GetCleanNumNewOpLogMeta() {
  size_t tmp = num_new_oplog_metas_;
  num_new_oplog_metas_ = 0;
  return tmp;
}

int32_t ValueTableOpLogMeta::GetAndClearNextInOrder() {
  auto max = HeapExtractMax();
  //LOG(INFO) << "OpMeta " << "r " << max.row_id << " x " << max.importance;
  return max.row_id;
}

int32_t ValueTableOpLogMeta::InitGetUptoClock(int32_t clock) {
  heap_walker_ = 0;
  clock_to_clear_ = clock;
  heap_last_ = (heap_size_) > 0 ? heap_size_ - 1 : -1;
  //LOG(INFO) << __func__ << " heap_size = " << heap_size_;
  return GetAndClearNextUptoClock();
}


int32_t ValueTableOpLogMeta::GetAndClearNextUptoClock() {
  while (heap_walker_ <= heap_last_) {
    int32_t clock = heap_[heap_walker_].clock;
    if (clock >= 0 && clock <= clock_to_clear_)
      break;
    heap_walker_++;
  }

  if (heap_walker_ > heap_last_) {
    int32_t heap_check = heap_last_;

    while (heap_check >= 0) {
      if (heap_[heap_check].clock == -1) {
        if (heap_check == heap_last_) {
          heap_last_--;
        } else {
          int32_t heap_valid = heap_check + 1;

          while (heap_check >= 0 && heap_[heap_check].clock == -1) {
            heap_check--;
          }
          int32_t copy_to = heap_check + 1;

          memmove(heap_.data() + copy_to,
                  heap_.data() + heap_valid,
                  (heap_last_ - heap_valid + 1) * sizeof(IndexRowOpLogMeta));

          heap_last_ -= (heap_valid - copy_to);
          if (heap_check < 0) break;
        }
      } else {
      }
      heap_check--;
    }

    heap_index_.assign(table_size_, -1);

    for (int32_t i = 0; i < heap_size_; ++i) {
      heap_index_[heap_[i].row_id] = i;
    }

    HeapBuildMaxHeap();
    return -1;
  }

  heap_size_--;
  heap_[heap_walker_].clock = -1;
  int32_t row_id = heap_[heap_walker_].row_id;
  heap_walker_++;
  return row_id;
}

size_t ValueTableOpLogMeta::GetNumRowOpLogs() const {
  return heap_size_;
}

}
