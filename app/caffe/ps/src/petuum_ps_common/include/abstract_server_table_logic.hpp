#pragma once

#include <petuum_ps_common/include/configs.hpp>
#include <petuum_ps/server/server_row.hpp>

namespace petuum {

typedef void (*ApplyRowBatchIncFunc)(
    const int32_t *column_ids,
    const void *updates, int32_t num_updates,
    ServerRow *server_row);

class AbstractServerTableLogic {
public:
  AbstractServerTableLogic() { }
  virtual ~AbstractServerTableLogic() { }

  virtual void Init(const TableInfo &table_info,
                    ApplyRowBatchIncFunc RowBatchInc) = 0;

  virtual void ServerRowCreated(int32_t row_id,
                                ServerRow *server_row) = 0;
  virtual void ApplyRowOpLog(
      int32_t row_id,
      const int32_t *col_ids, const void *updates,
      int32_t num_updates, ServerRow *server_row,
      uint64_t row_version, bool end_of_version) = 0;

  virtual void ServerRowSent(int32_t row_id, uint64_t version,
                             size_t num_clients) = 0;
  virtual bool AllowSend() = 0;
};
}
