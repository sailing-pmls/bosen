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

// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2013.12.13

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <vector>
#include <stdio.h>
#include <algorithm>

DEFINE_string(data_file, "",
    "File containing document in LibSVM format. Each document is a line.");

// Parse an LibSVM formatted file and print out the maximum faeture dimension.
int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  char *line = NULL, *ptr = NULL;
  size_t num_bytes;
  FILE *data_stream = fopen(FLAGS_data_file.c_str(), "r");
  CHECK_NOTNULL(data_stream);
  LOG(INFO) << "Reading from data file " << FLAGS_data_file;
  int num_data = 0;
  int max_feature_id = 0;
  bool convert_label_notice = false;
  while (getline(&line, &num_bytes, data_stream) != -1) {
    // stat of a word
    int32_t label, feature_id, feature_val;
    ptr = line; // point to the start
    sscanf(ptr, "%d", &label);
    // Check binary label is {0, 1} or {-1, 1}
    CHECK_LE(label, 1) << "data " << num_data << " has label " << label
      << " out of range.";
    CHECK_LE(-1, label) << "data " << num_data << " has label " << label
      << " out of range.";
    // Convert {0, 1} label to {-1, 1} labels.
    if (label == 0) {
      if (!convert_label_notice) {
        LOG(INFO) << "converting label 0 to -1";
        convert_label_notice = true;
      }
      label = -1;
    }
    while (*ptr != '\n') {
      while (*ptr != ' ') ++ptr; // goto next space
      // read a feature_id:feature_val pair
      sscanf(++ptr, "%d:%d", &feature_id, &feature_val);
      max_feature_id = std::max(feature_id, max_feature_id);
      while (*ptr != ' ' && *ptr != '\n') ++ptr; // goto next space or \n
    }
    ++num_data;
    LOG_IF(INFO, num_data % 200000 == 0) << "Read data " << num_data;
  }
  LOG(INFO) << "max_feature_id = " << max_feature_id;
  free(line);
  CHECK_EQ(0, fclose(data_stream)) << "Failed to close file " << FLAGS_data_file;

  LOG(INFO) << "Read " << num_data << " data from " << FLAGS_data_file;

  return 0;
}
