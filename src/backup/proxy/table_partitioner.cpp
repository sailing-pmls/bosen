#include "petuum_ps/proxy/table_partitioner.hpp"

namespace petuum {

TablePartitioner& TablePartitioner::GetInstance() {
  static TablePartitioner instance;
  return instance;
}

void TablePartitioner::Init(int32_t num_servers) {
  num_servers_ = num_servers;
}

int32_t TablePartitioner::GetRowAssignment(int32_t table_id, int32_t row_id) {
  return row_id % num_servers_;
}

}  // namespace petuum
