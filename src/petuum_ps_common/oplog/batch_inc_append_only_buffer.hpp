// Author: Jinliang
#pragma once

#include <stdint.h>
#include <petuum_ps_common/oplog/abstract_append_only_buffer.hpp>

namespace petuum {

/*
 * This is the append-only buffer type that's specially optimized for BATCH_INC
 * operations. The buffer memory is organized as a list of
 * (row_id,
 * number_updates, *int32_t*
 * col_ids,
 * updates) tuples.
 */

class BatchIncAppendOnlyBuffer : public AbstractAppendOnlyBuffer {
public:
  BatchIncAppendOnlyBuffer(int32_t thread_id, size_t capacity, size_t update_size):
      AbstractAppendOnlyBuffer(thread_id, capacity, update_size) { }

  virtual ~BatchIncAppendOnlyBuffer() { }

  // return true if succeed, otherwise false
  bool Inc(int32_t row_id, int32_t col_id, const void *delta);
  bool BatchInc(int32_t row_id, const int32_t *col_ids,
                const void *deltas, int32_t num_updates) ;
  bool DenseBatchInc(int32_t row_id, const void *updates, int32_t index_st,
                     int32_t num_updates);
  void InitRead();
  const void *Next(int32_t *row_id, int32_t const **col_ids,
                   int32_t *num_updates);
};

}
