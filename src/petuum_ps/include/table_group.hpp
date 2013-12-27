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

/*
 * petuum_tabegroup.hpp
 *
 *  Created on: Sep 21, 2013
 *      Author: jinliang
 */

#ifndef __PETUUM_TABLEGROUP_HPP__
#define __PETUUM_TABLEGROUP_HPP__

#include "petuum_ps/include/table.hpp"
#include "petuum_ps/proxy/client_proxy.hpp"

#include <boost/unordered_map.hpp>
#include <boost/thread.hpp>
#include <boost/scoped_ptr.hpp>
#include <pthread.h>
#include <vector>
#include <zmq.hpp>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sstream>
#include <algorithm>    // std::max

namespace petuum {

/*
 * This class represents the configuration parameters that TableGroup may take.
 * This is used only when the TableGroup is constructed.
 * */
struct TableGroupConfig {
  int32_t client_id;
  int32_t num_threads;  // number of execution threads on this client.
  std::vector<ServerInfo> servers_;
  zmq::context_t *zmq_ctx_;

  TableGroupConfig(int32_t id, int num_threads, std::vector<ServerInfo>& ser,
		   zmq::context_t *ctx = NULL)
    : client_id(id), num_threads(num_threads), servers_(ser), zmq_ctx_(ctx) {
      CHECK_GT(num_threads, 0)
        << "Number of executing threads has to be >0 on a client.";
    }
};

/*
 * This class represents the handle to parameter server.  All tables must be
 * created from TableGroup.  This is different from LazyTables as Table itself
 * is the highest-level user interface and user may create a Table object
 * directly.  We need a handle to server as there might be server-wised
 * configuration to be set.  
 */
template <typename V>
class TableGroup {
public:
  static TableGroup<V>& CreateTableGroup(
      const TableGroupConfig &table_group_config);

  ~TableGroup();

  void ShutDown();

  // Creates a Table object t. t.errcode_ is set to 1 in case of failure.
  // When the Table is created, it is unique across all clients.
  // Thread-safe. Note that CreateDenseTable has to be called before
  // RegisterExecutionThread so all tables recognize all threads.
  DenseTable<V>& CreateDenseTable(int32_t table_id, TableConfig& table_config);

  // Iterate all tables in the group.
  void Iterate();

  // All threads need to call RegisterThread().
  int RegisterThread();
  int DeregisterThread();

  // Only execution threads (threads that accesses the table) can call
  // RegisterExecutionThread(). Return 0 on success, negatives on error.
  int RegisterExecutionThread();

  int ProcessBarrier();

  // All threads on all clients will be synchronized at this call. This
  // flushes the oplog, and invalidate all thread cache, and process cache.
  //
  // Comment(wdai): Currently we keep track of the maximum staleness and
  // Iterate() all tables max_staleness_ + 1 times.
  void GlobalBarrier();

private:
  TableGroup(const TableGroupConfig &table_group_config);
  void operator=(TableGroupConfig const&);

private:
  TableGroupConfig config_;

  typedef boost::unordered_map<int32_t, DenseTable<V>*> table_group_t;
  table_group_t tables_;
  mutable boost::shared_mutex mutex_; // mutex for the entire table group

  boost::scoped_ptr<CommHandler> comm_;
  boost::scoped_ptr<ClientProxy<V> > proxy_;

  // Maximum staleness among the tables.
  int32_t max_staleness_;
};

template<typename V>
TableGroup<V>& TableGroup<V>::CreateTableGroup(
    const TableGroupConfig &table_group_config)
{
  static TableGroup<V> table_group(table_group_config);
  return table_group;
}

template <typename V>
TableGroup<V>::TableGroup(const TableGroupConfig &table_group_config)
  : config_(table_group_config), max_staleness_(0) {

  // create and init comm handler
  ConfigParam commconf(table_group_config.client_id, false, "neverused",
      "neverused");
  comm_.reset(new CommHandler(commconf));
  comm_->Init(table_group_config.zmq_ctx_);

  // create client proxy
  ClientProxyConfig proxy_config;
  proxy_config.servers_ = table_group_config.servers_;
  proxy_.reset(new ClientProxy<V>(*comm_, proxy_config));
  proxy_->Initialize();
}

template<typename V>
TableGroup<V>::~TableGroup()
{
  typename table_group_t::iterator it;
  for (it = tables_.begin(); it != tables_.end(); ++it) {
    delete it->second;
  }
}

template <typename V>
void TableGroup<V>::ShutDown()
{
  comm_->ShutDown();
  comm_.reset();
}


template <typename V>
DenseTable<V>& TableGroup<V>::CreateDenseTable(int32_t table_id, 
    TableConfig& table_config) {
  
  boost::unique_lock<boost::shared_mutex> write_lock(mutex_);

  if (tables_.find(table_id) != tables_.end()) {
    // found
    LOG(WARNING) << "Ignoring attempt to create existing table (table_id = "
      << table_id << ")";
    return *(tables_[table_id]);
  }
  // not found
  int ret = proxy_->RequestCreateTableRPC(table_id, table_config);

  // Inject num_threads (a run time information) before creating table.
  table_config.num_threads = config_.num_threads;
  table_config.num_servers = config_.servers_.size();
  DenseTable<V> *table = new DenseTable<V>(table_id, table_config, *proxy_);

  tables_[table_id] = table;

  max_staleness_ = std::max(max_staleness_, table_config.table_staleness);
  return *table;
}

template <typename V>
void TableGroup<V>::Iterate() {

  boost::unique_lock<boost::shared_mutex> write_lock(mutex_);

  typename table_group_t::iterator it;
  for (it = tables_.begin(); it != tables_.end(); it++) {
    it->second->Iterate();
  }
}

template <typename V>
int TableGroup<V>::RegisterThread() {
  return proxy_->RegisterThread();
}

template <typename V>
int TableGroup<V>::DeregisterThread() {
  return proxy_->DeregisterThread();
}

template <typename V>
int TableGroup<V>::RegisterExecutionThread()
{
  for(auto& table : tables_) {
    table.second->RegisterThread();
  }
  return 0;
}

template <typename V>
int TableGroup<V>::ProcessBarrier()
{
  LOG(FATAL) << "Not implemented.";
  return -1;
}

template <typename V>
void TableGroup<V>::GlobalBarrier() {
  VLOG(0) << "Iterating " << max_staleness_ + 1
    << " times to simulate GlobalBarrier()";
  for (int i = 0; i < max_staleness_ + 1; ++i) {
    Iterate();
  }
}

}

#endif /* PETUUM_TABEGROUP_HPP_ */
