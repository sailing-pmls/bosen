// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.11.15

#include <snappy.h>
#include <fstream>
#include <string>
#include <cstdint>
#include <vector>
#include <dirent.h>
#include <algorithm>
#include <glog/logging.h>
#include <petuum_ps_common/util/high_resolution_timer.hpp>
#include <ml/disk_stream/multi_buffer.hpp>
#include <ml/disk_stream/byte_buffer.hpp>
#include <ml/disk_stream/disk_reader.hpp>

namespace petuum {
namespace ml {

DiskReader::DiskReader(const DiskReaderConfig& config,
    MultiBuffer* multi_buffer) :
  file_counter_(0), pass_counter_(0),
  multi_buffer_(CHECK_NOTNULL(multi_buffer)),
  // DiskReaderConfig parameters
  snappy_compressed_(config.snappy_compressed),
  num_passes_(config.num_passes),
  read_mode_(config.read_mode),
  dir_path_(config.dir_path),
  file_list_(config.file_list),
  seq_id_begin_(config.seq_id_begin),
  num_files_(config.num_files),
  file_seq_prefix_(config.file_seq_prefix) {
    GenerateFileList();
  }

void DiskReader::Start() {
  ByteBuffer* buffer_ptr = multi_buffer_->GetIOBuffer();
  petuum::HighResolutionTimer read_timer;
  int64_t num_bytes_read = 0;
  while (buffer_ptr != 0 &&
      (num_passes_ == 0 || pass_counter_ < num_passes_)) {
    // 0 is shutdown signal.
    std::vector<char> file_bytes = ReadNextFile();
    num_bytes_read += file_bytes.size();
    buffer_ptr->SetBuffer(&file_bytes);
    multi_buffer_->DoneFillingIOBuffer();
    buffer_ptr = multi_buffer_->GetIOBuffer();
  }
  multi_buffer_->IOThreadShutdown();
  int num_secs = read_timer.elapsed();
  LOG(INFO) << "Reader thread read " << num_bytes_read << " bytes in "
    << num_secs << " seconds ("
    << static_cast<float>(num_bytes_read) / 1e6 / num_secs << "MB/s)";
}

namespace {

// Generate file list from directory.
std::vector<std::string> ReadDir(const std::string& dir_path) {
  std::vector<std::string> files;
  struct dirent *entry;
  DIR *dp = opendir(dir_path.c_str());
  CHECK_NOTNULL(dp);

  while ((entry = readdir(dp))) {
    std::string filename = std::string(entry->d_name);
    if (filename.at(0) != '.') {
      // filename is not a hidden file. Ignore Windows system's filepath.
      files.push_back(dir_path + "/" + filename);
    }
  }
  closedir(dp);
  // Sort it to avoid random order and make it easier to test.
  std::sort(files.begin(), files.end());
  return files;
}

// Generate file list from reading a file list.
std::vector<std::string> ReadFileList(const std::string& file_list) {
  std::vector<std::string> files;
  std::ifstream is(file_list);
  CHECK(is) << "Failed to open " << file_list;
  std::string line;
  while (std::getline(is, line)) {
    files.push_back(line);
  }
  return files;
}

// Generate file list from a file sequence.
std::vector<std::string> ReadFileSequence(
    const std::string& file_seq_prefix, int seq_id_begin, int num_files) {
  CHECK_GT(num_files, 0) << "no file to read.";
  std::vector<std::string> files;
  for (int i = 0; i < num_files; ++i) {
    files.push_back(file_seq_prefix + std::to_string(seq_id_begin + i));
  }
  return files;
}

}  // anonymous namespace

void DiskReader::GenerateFileList() {
  if (read_mode_ == kDirPath) {
    files_ = ReadDir(dir_path_);
    CHECK(!files_.empty()) << "No file in directory " << dir_path_;
  } else if (read_mode_ == kFileList) {
    files_ = ReadFileList(file_list_);
  } else if (read_mode_ == kFileSequence) {
    files_ = ReadFileSequence(file_seq_prefix_, seq_id_begin_,
        num_files_);
  } else {
    LOG(FATAL) << "Unrecognized read_mode: " << read_mode_;
  }
  LOG(INFO) << "Reader thread will read from " << files_.size()
    << " files";
}

std::vector<char> DiskReader::ReadNextFile() {
  std::string filename = files_[file_counter_];
  std::ifstream ifs(filename, std::ios::binary | std::ios::ate);
  CHECK(ifs) << "Failed to open " << filename;
  std::ifstream::pos_type pos = ifs.tellg();

  std::vector<char> result(pos);
  ifs.seekg(0, std::ios::beg);
  ifs.read(result.data(), pos);

  if (snappy_compressed_) {
    // Snappy-decompress.
    std::string uncompressed;
    CHECK(snappy::Uncompress(result.data(), result.size(), &uncompressed))
      << "Cannot decompress with snappy. File might be corrupted.";
    result = std::vector<char>(uncompressed.begin(), uncompressed.end());
  }

  ++file_counter_;
  if (file_counter_ % files_.size() == 0) {
    ++pass_counter_;
    file_counter_ = 0;
  }
  return result;
}

}  // namespace ml
}  // namespace petuum
