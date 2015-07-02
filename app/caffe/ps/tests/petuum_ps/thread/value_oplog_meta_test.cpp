#include <petuum_ps/thread/value_table_oplog_meta.hpp>
#include <gtest/gtest.h>
#include <stdlib.h>
#include <iostream>

using namespace petuum;

class ValueOpLogMetaTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    value_oplog_meta = new ValueTableOpLogMeta(0, 100);
  }

  virtual void TearDown() {
    delete value_oplog_meta;
  }

  ValueTableOpLogMeta *value_oplog_meta;
};

TEST_F(ValueOpLogMetaTest, Basic) {
  srand(0);
  std::vector<double> importance_vec(100);
  RowOpLogMeta row_oplog_meta1, row_oplog_meta2;
  row_oplog_meta1.set_clock(0);
  row_oplog_meta2.set_clock(1);

  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 9; ++j) {
      int32_t row_id = i*10 + j;
      double importance = rand() % 100;
      row_oplog_meta1.set_importance(importance);
      value_oplog_meta->InsertMergeRowOpLogMeta(row_id, row_oplog_meta1);
      importance_vec[row_id] = importance;
    }
    for (int j = 9; j < 10; ++j) {
      int32_t row_id = i*10 + j;
      double importance = rand() % 100;
      row_oplog_meta2.set_importance(importance);
      value_oplog_meta->InsertMergeRowOpLogMeta(row_id, row_oplog_meta2);
      importance_vec[row_id] = importance;
    }
  }

  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 9; ++j) {
      int32_t row_id = i*10 + j;
      double importance = rand() % 100;
      row_oplog_meta1.set_importance(importance);
      value_oplog_meta->InsertMergeRowOpLogMeta(row_id, row_oplog_meta1);
      importance_vec[row_id] += importance;
    }
    for (int j = 9; j < 10; ++j) {
      int32_t row_id = i*10 + j;
      double importance = rand() % 100;
      row_oplog_meta2.set_importance(importance);
      value_oplog_meta->InsertMergeRowOpLogMeta(row_id, row_oplog_meta2);
      importance_vec[row_id] += importance;
    }
  }

  int32_t row_id = value_oplog_meta->InitGetUptoClock(0);
  size_t count = 0;
  while (row_id >= 0) {
    std::cout << "Clock count = " << count
              << " row_id " << row_id
              << " importacne = " << importance_vec[row_id] << std::endl;
    row_id = value_oplog_meta->GetAndClearNextUptoClock();
    count++;
  }

  std::cout << "Clock Done!" << std::endl;

  row_id = value_oplog_meta->GetAndClearNextInOrder();

  while (row_id >= 0) {
    std::cout << "InOrder count = " << count
              << " row_id " << row_id
              << " importacne = " << importance_vec[row_id] << std::endl;
    row_id = value_oplog_meta->GetAndClearNextInOrder();
    count++;
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
