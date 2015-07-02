#include <iostream>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include "io/general_fstream.hpp"
#include "io/hdfs.hpp"

DEFINE_string(hdfs_input, "", "path to input file on hdfs. Start without slash /");
DEFINE_string(hdfs_output, "", "path to input file on hdfs. Start without slash /");

/*
   COMPATIBLE with Hadoop 2.4.0, 2.5.2, 2.6.0 and the latest 2.7.0
   The code has been tested, and can be compiled, built, executed in hadoop 2.4.0, 2.5.2, 2.6.0, 2.7.0.
   */

namespace {

void TestHDFS() {
  std::string line;
  LOG(INFO) << "HDFS test: Reading from hdfs://localhost:9000/"
    + FLAGS_hdfs_input + " and then write twice the content to "
    + "hdfs://localhost:9000/" + FLAGS_hdfs_output;

  // please use full hdfs path like hdfs://host:port/path the SHORT PATH like
  // *hdfs:///input.txt* with default hdfs connection did NOT WORK in my test
  // in hadoop 2.4 and 2.5.2.  and you can use SHORT PATH ub hadoop 2.6.0 and
  // 2.7.0
  petuum::io::ifstream hdfs_in_file("hdfs://localhost:9000/"
      + FLAGS_hdfs_input);
  petuum::io::ofstream hdfs_out_file("hdfs://localhost:9000/"
      + FLAGS_hdfs_output);

  hdfs_out_file.exceptions(std::ios::failbit);
  while(!hdfs_in_file.eof()){
    std::getline(hdfs_in_file, line);
    LOG(INFO) << "Read: " << line;
    hdfs_out_file << line << std::endl;
  }
  hdfs_in_file.clear();
  hdfs_in_file.seekg(0, std::ios_base::beg);
  while(!hdfs_in_file.eof()){
    std::getline(hdfs_in_file, line);
    hdfs_out_file << line << std::endl;
  }
  hdfs_in_file.close();
  hdfs_out_file.close();
  LOG(INFO) << "HDFS test succeeded!";
}

void TestLocal() {
  std::string line;
  LOG(INFO) << "LocalFS test: read from input.txt and then write to output.txt";

  petuum::io::ifstream in_file("input.txt");
  petuum::io::ofstream out_file("output.txt");
  while(!in_file.eof()){
    std::getline(in_file, line);
    out_file << line << std::endl;
  }
  in_file.clear();
  in_file.seekg(0, std::ios_base::beg);
  out_file.seekp(0, std::ios_base::beg);
  while(!in_file.eof()){
    std::getline(in_file, line);
    out_file << line << std::endl;
  }
  in_file.close();
  out_file.close();
  LOG(INFO) << "LocalFS test succeeded!";
}

}  // anonymous namespace

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  TestLocal();
  TestHDFS();
  return 0;
}
