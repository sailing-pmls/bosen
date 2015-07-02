// Author: Zhang Yangyang (zhangyy@act.buaa.edu.cn)

#include "general_fstream.hpp"
#include <glog/logging.h>
#include <boost/algorithm/string/predicate.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <tuple>
#include "hdfs.hpp"

namespace petuum {
namespace io {

/*
* parsing the url manually instead of useing regex.
*/
std::tuple<std::string, std::string, std::string> parse_hdfs_url2(
    const std::string &hdfs_url) {
  std::string host;
  std::string port;
  std::string path;
  const std::string hdfs_prot = "hdfs://";
  std::string url = hdfs_url;
  if (url.compare(0, hdfs_prot.size(), hdfs_prot) == 0) {
    url = url.substr(hdfs_prot.size(), url.size());
    int at = url.find(':');
    if (at >= 0) {
      host = url.substr(0, at);
      url = url.substr(at + 1, url.size());
      at = url.find('/');
      if (at <= 0)
        LOG(FATAL) << "invalide HDFS url : " << hdfs_url;
      port = url.substr(0, at);
      path = url.substr(at, url.size());
    } else {
      at = url.find('/');
      if (at != 0) {
        LOG(FATAL) << "invalide HDFS url : " << hdfs_url;
      }
      path = url;
    }
  } else {
    LOG(FATAL) << "invalide HDFS url : " << hdfs_url;
  }
  return std::make_tuple(host, port, path);
}

/* @zhangyy std regex is not full supported in g++<=4.8.2. using a higher
 * version of g++ or boost::regex may help.  but using boost library will
 * bring another libboost-regex.so.  both will be very annoying.  and to make
 * it work with more compiler, I use another function 'parse_hdfs_url2' to
 * parse the url
 */
// split the url into host port and path.
std::tuple<std::string, std::string, std::string>
  parse_hdfs_url(const std::string & url) {
  return parse_hdfs_url2(url);
}

std::streamsize general_device::read(char* s, std::streamsize n) {
  input_stream_->read(s, n);
  return input_stream_->gcount();
}

std::streamsize general_device::write(const char* s, std::streamsize n) {
  output_stream_->write(s, n);
  return n;
}

std::streampos general_device::seek(std::streamoff off,
  std::ios_base::seekdir way) {
  if (is_reading_) {
    if (input_stream_) {
      input_stream_->clear();
      input_stream_->seekg(off, way);
      return input_stream_->tellg();
    } else {
      return -1;
    }
  } else {
    if (output_stream_) {
      output_stream_->clear();
      output_stream_->seekp(off, way);
      return output_stream_->tellp();
    } else {
      return -1;
    }
  }
}

void general_device::close() {
  switch (device_type) {
    case HDFS:
      if (input_stream_)
        dynamic_cast<hdfs::fstream *>(input_stream_)->close();
      if (output_stream_)
        dynamic_cast<hdfs::fstream *>(output_stream_)->close();
      break;
    case LOCALFS:
      if (input_stream_)
        dynamic_cast<std::fstream *>(input_stream_)->close();
      if (output_stream_)
        dynamic_cast<std::fstream *>(output_stream_)->close();
      break;
  }
  if (input_stream_) {
    delete input_stream_;
    input_stream_ = nullptr;
  }
  if (output_stream_) {
    delete output_stream_;
    output_stream_ = nullptr;
  }
}

bool general_device::good() {
  if (is_reading_)
    return (input_stream_ && input_stream_->good());
  else
    return (output_stream_ && output_stream_->good());
}

bool general_device::open_file(const std::string &url,
  std::ios_base::openmode mode) {
  input_stream_ = nullptr;
  output_stream_ = nullptr;
  is_reading_ = false;

  CHECK(!(mode & std::ios_base::in) || !(mode & std::ios_base::out))
    << "open mode cannot be std::ios_base::in "
    << "| std::ios_base::out!";
  CHECK((mode & (std::ios_base::in | std::ios_base::out |
          std::ios_base::app)))
    << "open mode should have one of std::ios_base::in "
    << "or std::ios_base::out, app set!";

  if (mode & std::ios_base::in) {
    is_reading_ = true;
  }

  if (boost::starts_with(url, "hdfs://")) {
    device_type = HDFS;
    std::string host, port, path;
    std::tie(host, port, path) = parse_hdfs_url(url);
    if (!host.empty() && !port.empty()) {
      auto &hdfs = hdfs::get_hdfs(host, std::atoi(port.c_str()));
      if (is_reading_) {
        input_stream_ = new hdfs::fstream(hdfs, path, mode);
      } else {
        output_stream_ = new hdfs::fstream(hdfs, path, mode);
      }
    } else if (host.empty() && port.empty()) {
      if (is_reading_) {
        input_stream_ = new hdfs::fstream(hdfs::get_hdfs(), path, mode);
      } else {
        output_stream_ = new hdfs::fstream(hdfs::get_hdfs(), path, mode);
      }
    } else {
      LOG(FATAL) << "hdfs url format : hdfs://[host:port]/path !";
    }
  } else {
    device_type = LOCALFS;
    if (is_reading_) {
      input_stream_ = new std::fstream(url, mode);
    } else {
      output_stream_ = new std::fstream(url, mode);
    }
    CHECK(good()) << '\'' << url << "\': file open error!";
  }
  return good();
}

}   // namespace io
}   // namespace petuum
