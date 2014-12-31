// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2013.11.29

#ifndef PETUUM_TABLE_PARTITIONER
#define PETUUM_TABLE_PARTITIONER

#include <stdint.h>

namespace petuum {

// TablePartitioner is a singleton storing all partition information for all
// table.
class TablePartitioner {
  public:
    static TablePartitioner& GetInstance();

    // Initialize TablePartitioner.
    // TODO(wdai): Use config in the future.
    void Init(int32_t num_servers);

    // Get server id from table_id and row_id. Currently just simple mod.
    int32_t GetRowAssignment(int32_t table_id, int32_t row_id);

  private:
    // Defeat various constructors.
    TablePartitioner() { };
    TablePartitioner(TablePartitioner const&);
    void operator=(TablePartitioner const&);

    int32_t num_servers_;
};

}  // namespace petuum

#endif  // PETUUM_TABLE_PARTITIONER
