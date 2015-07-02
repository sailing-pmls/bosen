#include <gtest/gtest.h>

#include <petuum_ps_common/storage/dense_row.hpp>
#include <petuum_ps_common/storage/sparse_row.hpp>
#include <petuum_ps_common/storage/sorted_vector_map_row.hpp>

using namespace petuum;

class RowTest : public ::testing::Test {
protected:
  virtual void SetUp() {

  }

  virtual void TearDown() {
  }

  DenseRow<int> dense_row;
  SparseRow<int> sparse_row;
  SortedVectorMapRow<int> sorted_vector_map_row;
};

TEST_F(RowTest, RowInit) {
  CHECK_EQ(1, 1);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
