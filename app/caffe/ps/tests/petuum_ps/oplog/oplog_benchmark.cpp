#include <gflags/gflags.h>
#include <glog/logging.h>
#include <iostream>
#include <stdlib.h>
#include <time.h>

#include <petuum_ps_common/include/petuum_ps.hpp>
#include <petuum_ps/oplog/append_only_oplog.hpp>

DEFINE_int32(dense_row_capacity, 1, "dense row capacity");
DEFINE_int32(partitioned_oplog_capacity, 10000, "partitioned oplog capacity");
DEFINE_int32(num_batch_incs, 1000, "number of batch incs");

std::map<int32_t, petuum::HostInfo> host_map;
petuum::DenseRow<int32_t> dense_row;
petuum::TableOpLog *table_oplog;
petuum::ThreadAppendOnlyOpLog  *thread_append_only_oplog;
int32_t *col_ids;
int *updates;
int32_t *row_ids;

int main (int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  petuum::GlobalContext::Init(
      1,
      1,
      1,
      1,
      1,
      host_map,
      0,
      0,
      petuum::SSPAggr,
      false,
      -1,
      "",
      -1,
      "",
      petuum::Random,
      10,
      100,
      100,
      1,
      1,
      1,
      1,
      1,
      1);

  dense_row.Init(FLAGS_dense_row_capacity);

  table_oplog = new petuum::TableOpLog(
      0, FLAGS_partitioned_oplog_capacity, &dense_row,
      FLAGS_dense_row_capacity, petuum::RowOpLogType::kDenseRowOpLog);

  thread_append_only_oplog = new petuum::ThreadAppendOnlyOpLog(
      FLAGS_num_batch_incs*FLAGS_partitioned_oplog_capacity,
      &dense_row);

  col_ids = new int32_t[FLAGS_dense_row_capacity];
  updates = new int[FLAGS_dense_row_capacity];
  row_ids = new int32_t[FLAGS_num_batch_incs];

  for (int32_t i = 0; i < FLAGS_dense_row_capacity; ++i) {
    col_ids[i] = i;
    updates[i] = 1;
  }

  petuum::HighResolutionTimer timer;
  srand(time(NULL));
  for (int i = 0; i < FLAGS_num_batch_incs; ++i) {
    int32_t row_id = rand();
    row_ids[i] = row_id;
  }
  LOG(INFO) << "time to generate random row ids, sec = " << timer.elapsed();

  timer.restart();
  for (int i = 0; i < FLAGS_num_batch_incs; ++i) {
    table_oplog->BatchInc(row_ids[i], col_ids, updates, FLAGS_dense_row_capacity);
  }
  LOG(INFO) << "hash table based oplog, sec = " << timer.elapsed();

  timer.restart();
  for (int i = 0; i < FLAGS_num_batch_incs; ++i) {
    int ret = thread_append_only_oplog->BatchInc(row_ids[i], col_ids, updates,
                                                 FLAGS_dense_row_capacity);
    CHECK_EQ(ret, FLAGS_dense_row_capacity);
  }
  LOG(INFO) << "append only oplog, sec = " << timer.elapsed();

  delete table_oplog;
  delete[] row_ids;
  delete[] col_ids;
  delete[] updates;
}
