// Copyright (c) 2013, SailingLab
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the <ORGANIZATION> nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// Author: Dai Wei
// Date: 2013.12.13

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <algorithm>    // std::random_shuffle
#include <time.h>
#include <fstream>

// All these are required command line inputs
DEFINE_string(data_file, " ", "path to libsvm file.");
//DEFINE_string(vocab_file, " ", "path to vocab to int mapping.");
DEFINE_string(output_file, " ",
    "Results are in output_file.0, output_file.1, etc.");
DEFINE_int32(num_partitions, 0, "number of chunks to spit out");

// random generator function:
int myrandom (int i) { return rand() % i;}

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  if (FLAGS_data_file == " " || FLAGS_output_file == " "
      || FLAGS_num_partitions == 0) {
    LOG(FATAL)
      << "usage: need data_file, vocab_file, output_file, and num_partitions";
  }

  std::vector<std::string> lines;
  std::ifstream infile(FLAGS_data_file);
  std::string line;
  int num_lines = 0;
  while (std::getline(infile, line)) {
    lines.push_back(line);
    ++num_lines;
  }
  LOG(INFO) << "Read " << num_lines << " lines.";

  // Randomize lines
  srand(time(NULL));
  std::random_shuffle(lines.begin(), lines.end(), myrandom);

  int num_lines_per_partition = num_lines / FLAGS_num_partitions;

  // Output
  for (int i = 0; i < FLAGS_num_partitions; ++i) {
    std::ostringstream out_filename;
    out_filename << FLAGS_output_file << "." << i;
    std::ofstream outfile;
    outfile.open(out_filename.str());
    int line_begin = i * num_lines_per_partition;
    int line_end = (i == FLAGS_num_partitions - 1) ?
      num_lines : line_begin + num_lines_per_partition;
    for (int j = line_begin; j < line_end; ++j) {
      outfile << lines[j] << std::endl;
    }
    outfile.close();
    LOG(INFO) << "Wrote " << (line_end - line_begin) << " lines to "
      << out_filename.str();
  }

  return 0;
}
