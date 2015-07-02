#pragma once

#include <petuum_ps/oplog/row_oplog_meta.hpp>
#include <petuum_ps_common/include/abstract_row.hpp>
#include <petuum_ps_common/include/configs.hpp>

#include <stdint.h>
#include <boost/noncopyable.hpp>

namespace petuum {

class AbstractTableOpLogMeta : boost::noncopyable {
public:
  AbstractTableOpLogMeta() { }
  virtual ~AbstractTableOpLogMeta() { }

  virtual void InsertMergeRowOpLogMeta(int32_t row_id,
                                       const RowOpLogMeta& row_oplog_meta) = 0;
  virtual size_t GetCleanNumNewOpLogMeta() = 0;

  virtual void Prepare(size_t num_rows_to_send
                       __attribute__((unused)) ) { }
  // Assuming prepare has happened
  virtual int32_t GetAndClearNextInOrder() = 0;

  virtual int32_t InitGetUptoClock(int32_t clock) = 0;
  virtual int32_t GetAndClearNextUptoClock() = 0;

  virtual size_t GetNumRowOpLogs() const = 0;
};

}
