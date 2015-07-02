#pragma once
#include <petuum_ps_common/include/configs.hpp>

namespace petuum {
// user still need set thet following configuration parameters:
// 1) table_info.row_capacity
// 2) table_info.dense_row_oplog_capacity
// 3) proess_cache_capacity
// 4) thread_cache_capacity
// 5) oplog_capacity

void InitTableConfig(ClientTableConfig *config);
}
