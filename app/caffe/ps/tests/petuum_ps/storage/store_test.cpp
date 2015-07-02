#include <gtest/gtest.h>

#include <petuum_ps_common/storage/vector_store.hpp>
#include <petuum_ps_common/storage/map_store.hpp>
#include <petuum_ps_common/storage/sorted_vector_map_store.hpp>
using namespace petuum;

class StoreTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    vector_store = new VectorStore<int>();
    vector_store->Init(kInitSize);

    map_store = new MapStore<int>();
    map_store->Init(0);

    sorted_vector_map_store = new SortedVectorMapStore<int>();
    sorted_vector_map_store->Init(0);
  }

  virtual void TearDown() {
    delete vector_store;
    delete map_store;
    delete sorted_vector_map_store;
  }

  const size_t kInitSize = 100;
  AbstractStore<int> *vector_store;
  AbstractStore<int> *map_store;
  AbstractStore<int> *sorted_vector_map_store;
};

TEST_F(StoreTest, VectorInit) {
  EXPECT_EQ((dynamic_cast<VectorStore<int>*>(vector_store))->get_capacity(),
            kInitSize);
}

TEST_F(StoreTest, VectorGet) {
  EXPECT_EQ(vector_store->Get(5), 0);
}

TEST_F(StoreTest, VectorInc) {
  vector_store->Inc(1, 5);
  vector_store->Inc(2, 10);
  vector_store->Inc(1, -2);

  EXPECT_EQ(vector_store->Get(1), 3);
  EXPECT_EQ(vector_store->Get(2), 10);
}

TEST_F(StoreTest, VectorSerialize) {
  size_t serialized_size = vector_store->SerializedSize();
  EXPECT_EQ(serialized_size, sizeof(int)*kInitSize);

  uint8_t *mem = new uint8_t[serialized_size];
  vector_store->Serialize(mem);

  VectorStore<int> new_store;
  new_store.Deserialize(mem, serialized_size);
  delete[] mem;

  VectorStore<int>::Iterator iter;
  vector_store->RangeBegin(&iter, 0, kInitSize);

  size_t count = 0;

  for (; !iter.is_end(); ++iter) {
    count++;
    int32_t key = iter.get_key();
    EXPECT_EQ(*iter, new_store.Get(key));
  }

  EXPECT_EQ(count, kInitSize);
}

TEST_F(StoreTest, SGet) {
  EXPECT_EQ(sorted_vector_map_store->Get(100), 0);
}

TEST_F(StoreTest, SIncGet) {
  sorted_vector_map_store->Inc(1, 2);
  sorted_vector_map_store->Inc(2, 0);
  sorted_vector_map_store->Inc(3, -9);
  sorted_vector_map_store->Inc(15, 8);
  sorted_vector_map_store->Inc(3, 12);

  EXPECT_EQ(sorted_vector_map_store->Get(1), 2);
  EXPECT_EQ(sorted_vector_map_store->Get(2), 0);
  EXPECT_EQ(sorted_vector_map_store->Get(3), 3);
  EXPECT_EQ(sorted_vector_map_store->Get(4), 0);
  EXPECT_EQ(sorted_vector_map_store->Get(15), 8);
}

TEST_F(StoreTest, SShrink) {
  size_t size = 300;
  for (int i = 0; i < size; ++i) {
    sorted_vector_map_store->Inc(i, i % 17);
  }

  for (int i = 150; i >= 0; --i) {
    sorted_vector_map_store->Inc(i,  -(i % 17));
  }

  size_t num_entries = (dynamic_cast<SortedVectorMapStore<int>*>(
      sorted_vector_map_store))->num_entries();

  LOG(INFO) << "num_entries = " << num_entries;

  for (int i = 0; i <= 150; ++i) {
    EXPECT_EQ(sorted_vector_map_store->Get(i), 0);
  }

  for (int i = 151; i <= 150; ++i) {
    EXPECT_EQ(sorted_vector_map_store->Get(i), i % 17);
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
