// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.11.24

#include <gtest/gtest.h>
#include <glog/logging.h>
#include <vector>
#include <thread>
#include <atomic>
#include <ml/disk_stream/disk_streamer.hpp>
#include <ml/disk_stream/parsers/libsvm_parser.hpp>
#include <ml/feature/abstract_datum.hpp>
#include <ml/feature/sparse_feature.hpp>

namespace petuum {
namespace ml {

namespace {

const int32_t kNumData = 60;
const int32_t kNumDataPerParse = 1;

void Verify20News(DiskStreamer<AbstractDatum<int> >* streamer) {
  int i = 0;
  std::vector<AbstractDatum<int>*> parsed_data =
    streamer->GetNextData(kNumDataPerParse);
  while (!parsed_data.empty()) {
    ++i;
    EXPECT_EQ(kNumDataPerParse, parsed_data.size());
    AbstractDatum<int>* datum = parsed_data[0];
    SparseFeature<int>& sp_feature =
      *static_cast<SparseFeature<int>*>(datum->GetFeature());
    if (i == 1) {
      EXPECT_EQ(0, datum->GetLabel());
      EXPECT_EQ(0, sp_feature[0]);
      EXPECT_EQ(2, sp_feature[38]);
    }
    if (i == 3) {
      EXPECT_EQ(2, datum->GetLabel());
      EXPECT_EQ(3, sp_feature[1]);
    }
    delete parsed_data[0];
    parsed_data = streamer->GetNextData(kNumDataPerParse);
  }
  EXPECT_EQ(kNumData, i);
}

}  // anonymous namespace


TEST(DiskStreamTest, SingleWorkerFileSeqTests) {
  DiskStreamerConfig streamer_config;
  streamer_config.num_buffers = 2;
  streamer_config.num_worker_threads = 1;
  streamer_config.disk_reader_config.snappy_compressed = false;
  streamer_config.disk_reader_config.num_passes = 1;
  streamer_config.disk_reader_config.read_mode = kFileSequence;
  streamer_config.disk_reader_config.seq_id_begin = 0;
  streamer_config.disk_reader_config.num_files = 4;
  streamer_config.disk_reader_config.file_seq_prefix =
    "tests/ml/disk_stream/test_data/20news.";

  LibSVMParserConfig parser_config;
  parser_config.output_feature_type = kSparseFeature;
  parser_config.feature_dim = 53975;  // # of vocabs in 20news
  parser_config.feature_one_based = false;
  parser_config.label_one_based = false;

  // Read from file sequence.
  DiskStreamer<AbstractDatum<int> > streamer(streamer_config,
      new LibSVMParser<int>(parser_config));
  Verify20News(&streamer);

  // Read from directory.
  streamer_config.disk_reader_config.read_mode = kDirPath;
  streamer_config.disk_reader_config.dir_path =
    "tests/ml/disk_stream/test_data";
  DiskStreamer<AbstractDatum<int> > streamer_dir(streamer_config,
      new LibSVMParser<int>(parser_config));
  Verify20News(&streamer_dir);

  // Read from file list.
  streamer_config.disk_reader_config.read_mode = kFileList;
  streamer_config.disk_reader_config.file_list =
    "tests/ml/disk_stream/20news.filelist";
  DiskStreamer<AbstractDatum<int> > streamer_filelist(streamer_config,
      new LibSVMParser<int>(parser_config));
  Verify20News(&streamer_filelist);
}


namespace {

const int32_t kNumDataPerParse2 = 10;

// Sum of # of data processed by each thread.
std::atomic<int> data_count;

// Return # of data processed.
void WorkerThread(DiskStreamer<AbstractDatum<int> >* streamer) {
  int i = 0;
  std::vector<AbstractDatum<int>*> parsed_data =
    streamer->GetNextData(kNumDataPerParse2);
  i += parsed_data.size();
  while (!parsed_data.empty()) {
    for (auto& p : parsed_data) {
      delete p;
    }
    parsed_data = streamer->GetNextData(kNumDataPerParse2);
    i += parsed_data.size();
  }
  data_count += i;
}

// Return # of data processed.
void WorkerThreadSelfTerminate(DiskStreamer<AbstractDatum<int> >* streamer) {
  int i = 0;
  std::vector<AbstractDatum<int>*> parsed_data =
    streamer->GetNextData(kNumDataPerParse2);
  while (!parsed_data.empty()) {
    for (auto& p : parsed_data) {
      delete p;
    }
    parsed_data = streamer->GetNextData(kNumDataPerParse2);
    ++i;
    if (i == 1000) {
      streamer->Shutdown();
      break;
    }
  }
}

}  // anonymous namespace

TEST(DiskStreamTest, MultiWorkerTests) {
  data_count = 0;
  int num_worker_threads = 20;
  int num_passes = 10;
  DiskStreamerConfig streamer_config;
  streamer_config.num_buffers = 4;
  streamer_config.num_worker_threads = num_worker_threads;
  streamer_config.disk_reader_config.snappy_compressed = false;
  streamer_config.disk_reader_config.num_passes = num_passes;
  streamer_config.disk_reader_config.read_mode = kFileSequence;
  streamer_config.disk_reader_config.seq_id_begin = 0;
  streamer_config.disk_reader_config.num_files = 4;
  streamer_config.disk_reader_config.file_seq_prefix =
    "tests/ml/disk_stream/test_data/20news.";

  LibSVMParserConfig parser_config;
  parser_config.output_feature_type = kSparseFeature;
  parser_config.feature_dim = 53975;  // # of vocabs in 20news
  parser_config.feature_one_based = false;
  parser_config.label_one_based = false;

  // First test finite pass over data (IO thread shuts down worker threads).
  DiskStreamer<AbstractDatum<int> > streamer(streamer_config,
      new LibSVMParser<int>(parser_config));

  std::vector<std::thread> threads(num_worker_threads);
  for (auto& thr : threads) {
    thr = std::thread(&WorkerThread, &streamer);
  }
  for (auto& thr : threads) {
    thr.join();
  }
  LOG(INFO) << "===== All worker thread joined. # of docs processed: "
    << data_count << " =======";
  EXPECT_EQ(kNumData * num_passes, data_count);

  // Now shutdown from the worker thread.
  streamer_config.disk_reader_config.num_passes = 0;  // infinite stream.
  DiskStreamer<AbstractDatum<int> > streamer_infinite(streamer_config,
      new LibSVMParser<int>(parser_config));
  for (auto& thr : threads) {
    thr = std::thread(&WorkerThreadSelfTerminate, &streamer_infinite);
  }
  for (auto& thr : threads) {
    thr.join();
  }
  LOG(INFO) << "All worker threads joined.";
}

}  // namespace ml
}  // namespace petuum
