// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.11.15

#pragma once

#include <string>
#include <cstdint>
#include <ml/disk_stream/multi_buffer.hpp>

namespace petuum {
namespace ml {

enum ReadMode {
  kDirPath,     // Read all files under a directory (default).
  kFileList,    // Read all files in a file list.
  kFileSequence    // Read files with same prefix followed by #.
};

struct DiskReaderConfig {
  // True if the read data are compressed by Snappy.
  bool snappy_compressed = false;

  // Number of passes. 0 for infinite pass.
  int32_t num_passes = 1;

  // See ReadMode enum.
  ReadMode read_mode = kDirPath;

  // ============== kDirPath Parameters ===============
  // Will read all files under this directory. Cannot have subdirectory.
  std::string dir_path;

  // ============== kFileList Parameters ==============
  std::string file_list;

  // ============== kFileSequence Parameters =============
  // Read  file [seq_id_begin, seq_id_begin + num_files)
  int32_t seq_id_begin = 0;
  int32_t num_files = 0;
  // Full file path excluding the _id.
  std::string file_seq_prefix;
};


// DiskReader reads (and optionally snappy-decompress) a set of  files
// (usually 64MB each), one at a time, to a MultiBuffer.
class DiskReader {
public:
  // Does not take ownership of multi_buffer.
  DiskReader(const DiskReaderConfig& config, MultiBuffer* multi_buffer);

  // Read 'num_passes_' times over the  files (could be infinite loop).
  void Start();

private:    // private functions.
  // Figure out the list of files to read and store in files_.
  void GenerateFileList();

  // Read the next  file and snappy decompress it.
  // Comment (wdai): NRVO in C++ will avoid copying the returned vector.
  std::vector<char> ReadNextFile();

private:    // private members.
  // # of files read so far (wrapped around).
  int32_t file_counter_;

  // # of passes completed so far. It's 1 after first pass.
  int32_t pass_counter_;

  // Does not take ownership of multi_buffer_.
  MultiBuffer* multi_buffer_;

  // List of files to read.
  std::vector<std::string> files_;

  // See DiskReaderConfig.
  bool snappy_compressed_;
  int32_t num_passes_;
  ReadMode read_mode_;
  std::string dir_path_;
  std::string file_list_;
  int32_t seq_id_begin_;
  int32_t num_files_;
  std::string file_seq_prefix_;
};

}  // namespace ml
}  // namespace petuum
