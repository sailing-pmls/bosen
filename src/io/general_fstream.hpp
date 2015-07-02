// Author: Zhang Yangyang (zhangyy@act.buaa.edu.cn)

#pragma once

#include <boost/iostreams/stream.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <tuple>

namespace petuum {
namespace io {

// default buffer size is 16MB. you can ajust it for performance
const size_t BUFFER_SIZE = 16 * 1024 * 1024;
std::tuple<std::string, std::string, std::string>
  parse_hdfs_url(const std::string &url);

class general_device {
 public:
  enum device_type_t { HDFS, LOCALFS };
  std::streamsize read(char* s, std::streamsize n);
  std::streamsize write(const char* s, std::streamsize n);
  std::streampos seek(std::streamoff off, std::ios_base::seekdir way);
  void close();
  bool good();
  inline std::streamsize optimal_buffer_size() const { return BUFFER_SIZE; }

 protected:
  bool open_file(const std::string &url,
    std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out);

 protected:
  // HDFS or LOCALFS
  device_type_t device_type;

  // could be a hdfs::fstream ptr or std::ifstream ptr depending on device_type
  std::istream *input_stream_;

  // could be a hdfs::fstream ptr or std::ifstream ptr depending on device_type
  std::ostream *output_stream_;
  bool is_reading_;
};

class general_source : public general_device {
 public:
  // conceptions in boost::fstream
  typedef char char_type;

  struct category :
    public boost::iostreams::device_tag,
    public boost::iostreams::multichar_tag,
    public boost::iostreams::input_seekable,
    public boost::iostreams::closable_tag,
    public boost::iostreams::optimally_buffered_tag {};

  general_source() {}
  /** @url format hdfs://[host:port]/path or local filesystem absolute path
   * and relative path for compatibility of hdfs, please using full path like
   * hdfs://host:port/path is recommended
   */
  explicit general_source(const std::string &url,
      std::ios_base::openmode mode = std::ios_base::in) {
    open_file(url, mode | std::ios_base::in);
  }

  void open(const std::string &url,
      std::ios_base::openmode mode = std::ios_base::in) {
    close();
    open_file(url, mode | std::ios_base::in);
  }
};

class general_sink : public general_device {
 public:
  typedef char char_type;
  struct category :
    public boost::iostreams::device_tag,
    public boost::iostreams::multichar_tag,
    public boost::iostreams::output_seekable,
    public boost::iostreams::closable_tag,
    public boost::iostreams::optimally_buffered_tag {};

  general_sink() {}
  explicit general_sink(const std::string &url,
      std::ios_base::openmode mode = std::ios_base::out) {
    open_file(url, mode | std::ios_base::out);
  }

  void open(const std::string &url,
      std::ios_base::openmode mode = std::ios_base::out) {
    close();
    open_file(url, mode | std::ios_base::out);
  }
};

typedef boost::iostreams::stream<general_source> ifstream;
typedef boost::iostreams::stream<general_sink>   ofstream;

}   // namespace io
}   // namespace petuum
