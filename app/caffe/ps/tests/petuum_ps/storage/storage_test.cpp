#include <petuum_ps_common/storage/bounded_dense_process_storage.hpp>
#include <petuum_ps_common/util/thread.hpp>
#include <petuum_ps_common/storage/dense_row.hpp>
#include <stdlib.h>
#include <unistd.h>

#include <glog/logging.h>
#include <gflags/gflags.h>

DEFINE_int32(num_testers, 1, "num testers");
DEFINE_uint64(cache_capacity, 10, "cache capacity");
DEFINE_string(test_case, "InsertTest", "test case");

namespace petuum {

const int32_t kRowSize = 100;
const int32_t kNumInserts = 1000;

class Tester : public Thread {
public:
  enum TestCase {
    kInsert,
    kEvictionStress,
    kFindInsert
  };

  Tester(int32_t id, BoundedDenseProcessStorage *storage,
         TestCase test_case, size_t num_testers):
      id_(id),
      storage_(storage),
      test_case_(test_case),
      num_testers_(num_testers) { }

  ~Tester() { }

  void *operator() () {
    switch(test_case_) {
      case kInsert:
        {
          TestInsert();
        }
        break;
      case kEvictionStress:
        {
          TestEvictionStress();
        }
        break;
      case kFindInsert:
        {
          if (id_ % 2 == 0) {
            TestEvictionStress();
          } else {
            TestFind();
          }
        }
        break;
      default:
        LOG(FATAL) << "Unknown test case = " << test_case_;
    }
    return 0;
  }

private:
  void TestInsert() {
    DenseRow<int> *row_data = new DenseRow<int>;
    row_data->Init(kRowSize);
    ClientRow *client_row = new ClientRow(0, row_data, false);

    RowAccessor row_accessor;
    int32_t evicted_row_id = 0;
    storage_->Insert(id_, client_row, &row_accessor, &evicted_row_id);

    if (evicted_row_id >= 0)
      LOG(INFO) << "Evicted row, row id = " << evicted_row_id;
  }

  void TestEvictionStress() {
    for (int32_t i = 0; i < kNumInserts; ++i) {
      DenseRow<int> *row_data = new DenseRow<int>;
      row_data->Init(kRowSize);
      ClientRow *client_row = new ClientRow(0, row_data, false);

      RowAccessor row_accessor;
      int32_t evicted_row_id = 0;

      storage_->Insert(id_ + num_testers_*i, client_row, &row_accessor, &evicted_row_id);

      RowAccessor row_accessor_2;
      storage_->Find(id_ + num_testers_*i, &row_accessor_2);

      //if (evicted_row_id >= 0)
      //LOG(INFO) << "Evicted row, row id = " << evicted_row_id;
    }
  }

  void TestFind() {
    size_t num_rows  = num_testers_*kNumInserts;
    srand(id_);
    int32_t row_id = rand() % num_rows;

    for (int32_t i = 0; i < num_rows; ++i) {
      RowAccessor row_accessor;
      bool found = storage_->Find(row_id, &row_accessor);

      if (found) {
        LOG(INFO) << "Found row " << row_id << " ? " << found;
        usleep(1);
      }
      row_id = (row_id + 1) % num_rows;
    }
  }

  const int32_t id_;
  BoundedDenseProcessStorage *storage_;
  const TestCase test_case_;
  const size_t num_testers_;
};

ClientRow *CreateClientRow(int32_t clock) {
  DenseRow<int> *row_data = new DenseRow<int>;
  row_data->Init(kRowSize);
  ClientRow *client_row = new ClientRow(0, row_data, false);
  return client_row;
}

}

using namespace petuum;

int main (int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  Tester::TestCase test_case;

  if (FLAGS_test_case == "InsertTest") {
    test_case = Tester::kInsert;
  } else if (FLAGS_test_case == "EvictionStressTest") {
    test_case = Tester::kEvictionStress;
  } else if (FLAGS_test_case == "FindInsertTest") {
    test_case = Tester::kFindInsert;
  } else {
    LOG(FATAL) << "Unknown test = " << FLAGS_test_case;
  }

  size_t cache_capacity = FLAGS_cache_capacity;

  BoundedDenseProcessStorage process_storage(cache_capacity, CreateClientRow);

  std::vector<Tester*> testers(FLAGS_num_testers);
  for (int i = 0; i < FLAGS_num_testers; ++i) {
    testers[i] = new Tester(i, &process_storage, test_case, FLAGS_num_testers);
  }

  for (const auto &tester : testers) {
    tester->Start();
  }

  for (const auto &tester : testers) {
    tester->Join();
  }

  for (auto &tester : testers) {
    delete tester;
  }

  size_t num_rows_found = 0;
  for (int i = 0; i < FLAGS_num_testers*kNumInserts; ++i) {
    if (process_storage.Find(i))
      ++num_rows_found;
  }

  LOG(INFO) << "num rows found = " << num_rows_found;
  return 0;
}
