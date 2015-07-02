/*  
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
 * modified by zhangyy
 */
#include <iostream>
#include <sstream>
#include <string>
#include <mutex>
#include <map>
#include "hdfs.hpp"
static std::mutex mutex;
namespace petuum { namespace io {
#ifdef HAS_HADOOP
  hdfs& hdfs::get_hdfs() {
    static hdfs fs;
    return fs;
  }
    // a hdfs filesystem connection pool, file in the same hdfs
    // will use the same connection
    hdfs& hdfs::get_hdfs(const std::string& host, unsigned short port) {
        mutex.lock();
        static std::stringstream ss;
        static std::string key;
        ss.clear();
        ss << host << ":" << port;
        ss >> key;
        static std::map<std::string, std::shared_ptr<hdfs>> \
          hdfs_connection_pool;
        auto iter = hdfs_connection_pool.find(key);
        if (iter == hdfs_connection_pool.end()) {
            std::shared_ptr<hdfs> sp = std::make_shared<hdfs>(host, port);
            hdfs_connection_pool[key] = sp;
            mutex.unlock();
            return *sp;
        }
        mutex.unlock();
        return *iter->second;
    }
#else
    hdfs& hdfs::get_hdfs() {
      LOG(FATAL) << "Libhdfs is not installed on this system.";
    static hdfs fs;
    return fs;
  }
    hdfs& hdfs::get_hdfs(const std::string& host, unsigned short port) {
        return hdfs::get_hdfs();
    }
#endif
}  // namespace io
}  // namespace petuum
