#include <gflags/gflags.h>
#include <glog/logging.h>
#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <fstream>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_01.hpp>
#include <boost/random/variate_generator.hpp>
#include <petuum_ps_common/include/petuum_ps.hpp>

DEFINE_int32(row_oplog_type, petuum::RowOpLogType::kSparseVectorRowOpLog,
             "row oplog type");
DEFINE_uint64(row_capacity, 1000, "row capacity");
DEFINE_bool(oplog_dense_serialized, false, "dense serialize oplog");
DEFINE_string(append_only_oplog_type,"Inc", "append only oplog type");
DEFINE_uint64(append_only_buff_capacity, 1024*1024, "append only buffer capacity");
DEFINE_uint64(per_thread_append_only_buff_pool_size, 3, "buffer pool size");
DEFINE_int32(max_row_id, 1000, "max row id");
DEFINE_int32(num_iters, 5, "num iters");
DEFINE_int32(apply_freq, -1, "apply frequency");

typedef boost::variate_generator<
  boost::mt19937&,
  boost::uniform_01<> > rng_t;

std::map<int32_t, petuum::HostInfo> host_map;

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  petuum::PSTableGroup::RegisterRow<petuum::DenseRow<int32_t> >(0);

  petuum::GlobalContext::Init(
      1,
      1,
      1,
      1,
      1,
      host_map,
      0,
      0,
      petuum::SSPPush,
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

  size_t row_capacity = FLAGS_row_capacity;
  std::unordered_map<int32_t, petuum::AbstractRow*> storage;
  for (int32_t i = 0; i <= FLAGS_max_row_id; ++i) {
    petuum::AbstractRow *row_data
        = petuum::ClassRegistry<petuum::AbstractRow>::GetRegistry().CreateObject(0);
    row_data->Init(row_capacity);
    storage.insert(std::make_pair(i, row_data));
  }

  petuum::AppendOnlyOpLogType append_only_oplog_type;
  if (std::string("Inc").compare(FLAGS_append_only_oplog_type) == 0)
    append_only_oplog_type = petuum::Inc;
  else if (std::string("BatchInc").compare(FLAGS_append_only_oplog_type) == 0)
    append_only_oplog_type = petuum::BatchInc;
  else if (std::string("DenseBatchInc").compare(FLAGS_append_only_oplog_type) == 0)
    append_only_oplog_type = petuum::DenseBatchInc;
  else
    LOG(FATAL) << "Unknown append_only_oplog_type = " << FLAGS_append_only_oplog_type;

  size_t append_only_buff_capacity = FLAGS_append_only_buff_capacity;
  size_t per_thread_buff_pool_size = FLAGS_per_thread_append_only_buff_pool_size;

  petuum::AbstractRow *sample_row
      = petuum::ClassRegistry<petuum::AbstractRow>::GetRegistry().CreateObject(0);

  petuum::ThreadContext::RegisterThread(0);
  petuum::AppendOnlyOpLogPartition append_only_oplog_partition(
      append_only_buff_capacity, sample_row, append_only_oplog_type,
      row_capacity, per_thread_buff_pool_size);

  append_only_oplog_partition.RegisterThread();

  petuum::DenseUpdateBatch<int32_t> update_batch(0, row_capacity);
  for (int i = 0; i < row_capacity; ++i) {
    update_batch[i] = i % 5;
  }

  int32_t max_row_id = FLAGS_max_row_id;

  boost::uniform_01<> uniform_zero_one_dist;
  boost::mt19937 rng_engine;
  rng_t zero_one_rng(rng_engine, uniform_zero_one_dist);

  petuum::AppendOnlyRowOpLogBuffer append_only_row_oplog_buffer(
      FLAGS_row_oplog_type,
      sample_row, sizeof(int32_t), row_capacity, append_only_oplog_type);

  for (int iter = 0; iter < FLAGS_num_iters; ++iter) {
    size_t update_count = 0;
    //int32_t row_id = ((int32_t) (zero_one_rng() * (max_row_id + 1))) % max_row_id;
    int32_t row_id = 0;
    petuum::HighResolutionTimer timer;
    bool buff_full = append_only_oplog_partition.DenseBatchInc(
        row_id, update_batch.get_mem(), update_batch.get_index_st(),
        update_batch.get_num_updates());

    while (!buff_full && row_id < max_row_id) {
      ++update_count;
      //row_id = ((int32_t) (zero_one_rng() * (max_row_id + 1))) % max_row_id;
      ++row_id;
      buff_full = append_only_oplog_partition.DenseBatchInc(
          row_id, update_batch.get_mem(), update_batch.get_index_st(),
          update_batch.get_num_updates());
    }

    if (!buff_full)
      append_only_oplog_partition.FlushOpLog();

    double update_sec = timer.elapsed();
    LOG(INFO) << "iter = " << iter
              << " update_count = " << update_count
              << " update_sec = " << update_sec;

    timer.restart();
    petuum::AbstractAppendOnlyBuffer *buff
        = append_only_oplog_partition.GetAppendOnlyBuffer();

    int32_t num_updates;
    const int32_t *col_ids;
    buff->InitRead();
    const void *updates = buff->Next(&row_id, &col_ids, &num_updates);
    while (updates != 0) {
      append_only_row_oplog_buffer.BatchIncTmp(row_id, col_ids,
                                               updates, num_updates);
      updates = buff->Next(&row_id, &col_ids, &num_updates);
    }

    if (FLAGS_apply_freq > 0 &&
        (iter + 1) % FLAGS_apply_freq == 0) {
      petuum::AbstractRowOpLog *row_oplog
          = append_only_row_oplog_buffer.InitReadTmpOpLog(&row_id);
      while (row_oplog != 0) {
        auto row_iter = storage.find(row_id);
        if (row_iter != storage.end()) {
          petuum::AbstractRow *row_data = row_iter->second;
          row_data->GetWriteLock();
          int32_t column_id;
          const void *update;
          update = row_oplog->BeginIterateConst(&column_id);
          while (update != 0) {
            row_data->ApplyIncUnsafe(column_id, update);
            update = row_oplog->NextConst(&column_id);
          }
          row_data->ReleaseWriteLock();
        }
        row_oplog = append_only_row_oplog_buffer.NextReadTmpOpLog(&row_id);
      }
      append_only_row_oplog_buffer.MergeTmpOpLog();
    } else if (FLAGS_apply_freq < 0) {
      append_only_row_oplog_buffer.MergeTmpOpLog();
    }

    double process_sec = timer.elapsed();
    LOG(INFO) << "iter = " << iter
              << " process_sec = " << process_sec;
    append_only_oplog_partition.PutBackBuffer(buff);
  }

  {
    int32_t row_oplog_count = 0;
    int32_t row_id;
    petuum::AbstractRowOpLog *row_oplog
        = append_only_row_oplog_buffer.InitReadRmOpLog(&row_id);

    while (row_oplog != 0) {
      delete row_oplog;
      ++row_oplog_count;
      row_oplog = append_only_row_oplog_buffer.NextReadRmOpLog(&row_id);
    }

    LOG(INFO) << "row_oplog_count = " << row_oplog_count;
  }
  append_only_oplog_partition.DeregisterThread();

  std::vector<int32_t> data;
  std::ofstream output("test_out.txt", std::ofstream::out);
  for (auto &row_pair : storage) {
    petuum::DenseRow<int32_t> *row_data
        = reinterpret_cast<petuum::DenseRow<int32_t>* >(row_pair.second);
    output << "id:" << row_pair.first;
    row_data->CopyToVector(&data);
    for (int i = 0; i < data.size(); ++i) {
      output << " " << i << ":" << data[i];
    }
    output << std::endl;
  }
  output.close();
}
