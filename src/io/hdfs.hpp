/**  
 * Copyright (c) 2009 Carnegie Mellon University. 
 *     All rights reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing,
 *  software distributed under the License is distributed on an "AS
 *  IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 *  express or implied.  See the License for the specific language
 *  governing permissions and limitations under the License.
 *
 * For more about this software visit:
 *
 *      http://www.graphlab.ml.cmu.edu
 *
 */

/*
 * this file is a modification version of "src/graphlab/util/hdfs.hpp" from graphlab
 * modified by zhangyy
 * the original version of hdfs.hpp from graphlab will not work properly on seekg()
 * so we add seek function to support seekg()
 */
#ifndef __PETUUM_UTIL_HDFS_HPP_
#define __PETUUM_UTIL_HDFS_HPP_

// Requires the hdfs library
#ifdef HAS_HADOOP
extern "C" {
  #include "hdfs.h"
}
#endif
#include <boost/iostreams/stream.hpp>
#include <glog/logging.h>
#include <stdint.h>
#include <cassert>
#include <vector>
#include <string>

namespace petuum { namespace io {
#ifdef HAS_HADOOP
class hdfs {
 private:
  hdfsFS filesystem_;

 public:
  class hdfs_seekable_device {
   public:  // boost iostream concepts
    typedef char char_type;
    struct category :
      public boost::iostreams::seekable_device_tag,
      public boost::iostreams::multichar_tag,
      public boost::iostreams::closable_tag { };

   private:
    hdfsFS filesystem_;
    hdfsFile file_;
    std::string filepath_;

   public:
    hdfs_seekable_device() : filesystem_(NULL), file_(NULL) { }
    hdfs_seekable_device(const hdfs& hdfs_fs, const std::string& filename,
                    std::ios_base::openmode mode = std::ios_base::in|std::ios_base::out) :
                    filesystem_(hdfs_fs.filesystem_), filepath_(filename) {
      CHECK_NOTNULL(filesystem_);
      int32_t flags = 0;
      const int32_t buffer_size = 0;  // use default
      const int16_t replication = 0;  // use default
      const tSize block_size = 0;  // use default;

      // TODO check if mode is valid and supported by HDFS
      // in, out, app, out | app are supported
      // map mode to flags
      if (mode & std::ios_base::in) {
        flags = O_RDONLY;
      } else if (mode & std::ios_base::out) {
        flags = O_WRONLY;
        if (mode & std::ios_base::app) {
          flags |= O_APPEND;
        }
      } else if (mode & std::ios_base::app) {
          flags = O_WRONLY|O_APPEND;
      }
      if (flags & O_APPEND) {
        //
        if (hdfsExists(filesystem_, filename.c_str()) != 0) {
          // create file first!
          file_ = hdfsOpenFile(filesystem_, filename.c_str(), O_WRONLY, buffer_size,
                          replication, block_size);
          if (file_ == nullptr) {
            LOG(FATAL) << "file_ '" << filename <<"' open failed.(O_WRONLY)!";
          }
          hdfsCloseFile(filesystem_, file_);
          file_ = nullptr;
        }
      }
      file_ = hdfsOpenFile(filesystem_, filename.c_str(), flags, buffer_size,
                          replication, block_size);
      // check file is valid
      if (!file_)
        LOG(FATAL) << "file '" << filename <<"' does NOT exist!";
    }
    hdfs_seekable_device(const hdfs& hdfs_fs, const std::string& filename,
                    const bool write = false) :
          filesystem_(hdfs_fs.filesystem_), filepath_(filename) {
      CHECK_NOTNULL(filesystem_);
      const int32_t flags = write? O_WRONLY : O_RDONLY;
      const int32_t buffer_size = 0;  // use default
      const int16_t replication = 0;  // use default
      const tSize block_size = 0;  // use default;
      file_ = hdfsOpenFile(filesystem_, filename.c_str(), flags, buffer_size,
                          replication, block_size);
      // check file is valid
      if (!file_)
        LOG(FATAL) << "file '" << filename <<"' does NOT exist!";
    }
    // ~hdfs_device() { if(file != NULL) close(); }
    void close(std::ios_base::openmode mode = std::ios_base::openmode()) {
      if (file_ == NULL) return;
      if (hdfsFileIsOpenForWrite(file_)) {
        const int flush_error = hdfsFlush(filesystem_, file_);
        CHECK_EQ(0, flush_error);
      }
      const int close_error = hdfsCloseFile(filesystem_, file_);
      CHECK_EQ(0, close_error);
      file_ = NULL;
    }  // end of close

    // to support seek operation on file_ for read
    boost::iostreams::stream_offset seek(boost::iostreams::stream_offset off,
      std::ios_base::seekdir way) {
      boost::iostreams::stream_offset pos = 0;
      boost::iostreams::stream_offset size = 0;
      hdfsFileInfo *fileInfo = NULL;
      int error;
      if (hdfsFileIsOpenForWrite(file_)) {
        // hdfs does NOT support seek operation on file for write
        LOG(FATAL) << "seek operation unsupported on HDFS files for write";
      }
      // to get the size of a file, we need to get the struct hdfsfileInfo first
      fileInfo = hdfsGetPathInfo(filesystem_, filepath_.c_str());
      CHECK_NOTNULL(fileInfo);
      size = fileInfo->mSize;
      hdfsFreeFileInfo(fileInfo, 1);
      if (way == std::ios_base::cur) {
        pos = hdfsTell(filesystem_, file_);
        CHECK_GE(pos, 0);
      } else if (way == std::ios_base::beg) {
        pos = 0;
      } else if (way == std::ios_base::end) {
        pos = size;
      } else {
        LOG(FATAL) << "bad seek direction";
      }
      pos += off;
      if (pos < 0 || pos > size) {
        LOG(FATAL) << "bad seek offset";
      }
      error = hdfsSeek(filesystem_, file_, pos);
      CHECK_EQ(0, error);
      return pos;
    }  // end of seek

    inline std::streamsize optimal_buffer_size() const { return 0; }

    std::streamsize read(char* strm_ptr, std::streamsize n) {
      return hdfsRead(filesystem_, file_, strm_ptr, n);
    }  // end of read

    std::streamsize write(const char* strm_ptr, std::streamsize n) {
      return hdfsWrite(filesystem_, file_, strm_ptr, n);
    }
    bool good() const { return file_ != NULL; }
  };  // class hdfs_seekable_device
  typedef boost::iostreams::stream<hdfs_seekable_device> fstream;

  explicit hdfs(const std::string& host = "default", tPort port = 0) {
    filesystem_ =  hdfsConnect(host.c_str(), port);
    if (filesystem_ == nullptr) {
      LOG(FATAL) << "Cannot connect to HDFS. Host: " << host << ", port: " << port;
    }
  }  // end of constructor

  ~hdfs() {
    const int error = hdfsDisconnect(filesystem_);
    assert(error == 0);
  }  // end of ~hdfs

  inline std::vector<std::string> list_files(const std::string& path) {
    int num_files = 0;
    hdfsFileInfo* hdfs_file_list_ptr = \
      hdfsListDirectory(filesystem_, path.c_str(), &num_files);
    // copy the file list to the string array
    std::vector<std::string> files(num_files);
    for (int i = 0; i < num_files; ++i)
      files[i] = std::string(hdfs_file_list_ptr[i].mName);
      // free the file list pointer
    hdfsFreeFileInfo(hdfs_file_list_ptr, num_files);
    return files;
  }  // end of list_files

  inline static bool has_hadoop() { return true; }

  static hdfs& get_hdfs();

  static hdfs& get_hdfs(const std::string& host, unsigned short port);
};  // class hdfs
#else
class hdfs {
 public:
  /** hdfs file source is used to construct boost iostreams */
  class hdfs_seekable_device {
   public:  // boost iostream concepts
    typedef char                                          char_type;
    typedef boost::iostreams::bidirectional_device_tag    category;
   public:
    hdfs_seekable_device(const hdfs& hdfs_fs, const std::string& filename,
                    std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out) {
      LOG(FATAL) << "Libhdfs is not installed on this system.";
    }
    hdfs_seekable_device(const hdfs& hdfs_fs, const std::string& filename,
                  const bool write = false) {
      LOG(FATAL) << "Libhdfs is not installed on this system.";
    }
    void close() { }
    std::streamsize read(char* strm_ptr, std::streamsize n) {
      LOG(FATAL) << "Libhdfs is not installed on this system.";
      return 0;
    }  // end of read
    std::streamsize write(const char* strm_ptr, std::streamsize n) {
      LOG(FATAL) << "Libhdfs is not installed on this system.";
      return 0;
    }
    bool good() const { return false; }
  };  // end of hdfs_seekable_device

  /**
   * The basic file type has constructor matching the hdfs device.
   */
  typedef boost::iostreams::stream<hdfs_seekable_device> fstream;

  /**
   * Open a connection to the filesystem. The default arguments
   * should be sufficient for most uses
   */
  explicit hdfs(const std::string& host = "default", int port = 0) {
    LOG(FATAL) << "Libhdfs is not installed on this system.";
  }  // end of constructor

  inline std::vector<std::string> list_files(const std::string& path) {
    LOG(FATAL) << "Libhdfs is not installed on this system.";
    return std::vector<std::string>();;
  }  // end of list_files

  // No hadoop available
  inline static bool has_hadoop() { return false; }
  static hdfs& get_hdfs();
  static hdfs& get_hdfs(const std::string& host, unsigned short port);
};  // class hdfs

#endif
}  // namespace io
}  // namespace petuum

#endif
