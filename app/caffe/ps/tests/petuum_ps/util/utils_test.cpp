#include "petuum_ps/util/utils.hpp"
#include <map>
#include <gtest/gtest.h>

namespace petuum {

namespace {

}  // anonymous namespace

TEST(UtilsTest, ReadTableConfigs) {
  std::map<int, TableConfig> table_configs_map =
    ReadTableConfigs("tests/petuum_ps/util/test.cfg");

  EXPECT_EQ(2, table_configs_map.size());
  EXPECT_EQ(1, table_configs_map.count(1));
  EXPECT_EQ(1, table_configs_map.count(2));

  // Verify the first table config.
  TableConfig t1 = table_configs_map[1];
  EXPECT_EQ(100, t1.num_columns);
  EXPECT_EQ(3, t1.table_staleness);
  EXPECT_EQ(3, t1.process_storage_config.capacity);
  EXPECT_EQ(2, t1.op_log_config.thread_cache_capacity);
  EXPECT_EQ(10, t1.op_log_config.max_pending_op_logs);

  // Verify the second table config
  TableConfig t2 = table_configs_map[2];
  EXPECT_EQ(101, t2.num_columns);
  EXPECT_EQ(1, t2.table_staleness);
  EXPECT_EQ(4, t2.process_storage_config.capacity);
  EXPECT_EQ(4, t2.op_log_config.thread_cache_capacity);
  EXPECT_EQ(15, t2.op_log_config.max_pending_op_logs);
}

}  // namespace petuum
