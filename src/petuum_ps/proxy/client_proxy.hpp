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

#ifndef __PETUUM_CLIENT_PROXY__
#define __PETUUM_CLIENT_PROXY__

/* 
 * File: client_proxy.hpp
 * Authors: Jinliang, Wei Dai
 */

#include "petuum_ps/comm_handler/comm_handler.hpp"
#include "petuum_ps/include/configs.hpp"
#include "petuum_ps/storage/row_interface.hpp"
#include "petuum_ps/proxy/table_partitioner.hpp"
#include "protocol.hpp"

#include <boost/shared_array.hpp>
#include <boost/utility.hpp>
#include <glog/logging.h>

#include <vector>
#include <stdint.h>
#include <string>
#include <pthread.h>
#include <assert.h>

namespace petuum {

// Note: In all cases, (non-blocking) Recv functions return 1 when it actually
// receives something, 0 otherwise.

/*
* ClientProxy simulates RPC function calls by allowing client to send request 
* messages on the servers and receive messages from the servers.
* "RequestXXX" are all synchronous.
* All these functions are called by application threads. The class is 
* noncopyable.
*
* Current implementation uses CommHandler's thread-specific message. It 
* assumes:
* ClientProxy is the only trigger for thread-specific message. In other words, 
* CommHandler will not receive thread-specific message unless itself triggers a
*  thread-specific message.
*/

struct ServerInfo {
  int32_t id_;
  std::string ip_;
  std::string port_;
  ServerInfo(){}

  ServerInfo(int32_t _id, std::string _ip, std::string _port):
    id_(_id), ip_(_ip), port_(_port){}
};

struct ClientProxyConfig {
  std::vector<ServerInfo> servers_;

  ClientProxyConfig() : servers_(1) { }
};

template<typename V>
class ClientProxy : boost::noncopyable {
  public:

    ClientProxy(CommHandler &_comm, const ClientProxyConfig &_config);

    // Needed for gmock.
    virtual ~ClientProxy();

    // Note: Initialize is not thread-safe!!

    // connects to servers
    virtual int Initialize();

    // each application thread must register itself!
    // thread-safe
    virtual int RegisterThread();
    virtual int DeregisterThread();

    // Request server to create a Table whose ID is _table_id.
    // Different clients may invoke this function with the same _table_id
    // or even this function is invoked more than once on one client,
    // the server guarantees one unique table is created -- the first 
    // request seen by the server creates the table.
    // Return 0 or positive for success and negative for failure.
    // If 0, _table_config is not changed; if 1 _table_config is changed.
    virtual int RequestCreateTableRPC(int32_t _table_id, 
        TableConfig &_table_config);

    // Request to get a row from server, ClientProxy figures out which server to 
    // request.
    // Assumes memory pointed to by row is already allocated.
    // Return 0 for success, negative for failure.
    virtual int RequestGetRowRPC(int32_t table_id, int32_t row_id, Row<V> *row, 
        int32_t stalest_row_iter_);

    // This is called whenever OpLog hits a predefined limit even though the
    // program has not reached the end of an iteration.
    virtual int SendOpLogs(int32_t _table_id, int32_t _server_id,
        const boost::shared_array<uint8_t> &_op_log,
        int32_t _size);

    // Iterate a (SSP) table to all server partitions with op log. The op logs
    // for a table is serialized by OpLogManager's serialization. The function
    // internally determine the server_id based on row_id. _size is the number
    // of bytes in _op_log.
    virtual int SendIterate(int32_t _table_id, int32_t _server_id,
        const boost::shared_array<uint8_t> &_op_log,
        int32_t _size);

  private:
    // See constructor's comment on comm_handler_.
    CommHandler& comm_;
    std::vector<ServerInfo> servers_;
    int32_t name_node_id_;
    int32_t num_servers_;
    bool inited_;
    int32_t InitNameNode();

    pthread_mutex_t mtx_;
};

/*=============== Implementation ================*/
/*=============== Public Functions ==============*/
template<typename V>
ClientProxy<V>::ClientProxy(CommHandler &_comm, 
    const ClientProxyConfig &_config):
  comm_(_comm),
  servers_(_config.servers_),
  mtx_(PTHREAD_MUTEX_INITIALIZER),
  inited_(false) {
    name_node_id_ = InitNameNode();  
    num_servers_ = servers_.size();
    VLOG(1) << "ClientProxy created.";
  }

template<typename V>
ClientProxy<V>::~ClientProxy(){}

template<typename V>
int ClientProxy<V>::Initialize(){
  if(inited_) return -1;
  int suc;
  for (std::vector<ServerInfo>::iterator sit = servers_.begin(); 
      sit != servers_.end(); ++sit){
    suc = comm_.ConnectTo(sit->ip_, sit->port_, sit->id_);
    VLOG(0) << "******* Connected to server id = " << sit->id_ << " ip = " 
	      << sit->ip_  << " port =  "<< sit->port_;
    if(suc < 0) return -1;
  }
  inited_ = true;
  return 0;
}

template<typename V>
int ClientProxy<V>::RegisterThread(){
  pthread_mutex_lock(&mtx_);
  int ret = comm_.RegisterThr();
  pthread_mutex_unlock(&mtx_);
  return ret;
}

template<typename V>
int ClientProxy<V>::DeregisterThread(){
  pthread_mutex_lock(&mtx_);
  int ret = comm_.DeregisterThr();
  pthread_mutex_unlock(&mtx_);
  return ret;
}

template<typename V>
int ClientProxy<V>::RequestCreateTableRPC(int32_t _table_id, 
    TableConfig &_table_config){
  // Send CreateTable request to namenode only.
  SCCreateTableMsg msg(pthread_self(), _table_id, _table_config);
  int suc = comm_.Send(name_node_id_, (uint8_t *) &msg, 
      sizeof(SCCreateTableMsg));
  CHECK_EQ(suc, sizeof(SCCreateTableMsg));
  int32_t sid;
  boost::shared_array<uint8_t> data;
  suc = comm_.RecvThr(sid, data);
  CHECK_EQ(suc, sizeof(SCCreateTableReplyMsg));

  CreateTableReplyType ret = ((SCCreateTableReplyMsg *) data.get())->ret_;
  assert(ret != Failed);
  if(ret == TableExisted){
    _table_config = ((SCCreateTableReplyMsg *) data.get())->table_config_;
    return 1;
  }else{      
    return 0;
  }
}

template<typename V>
int ClientProxy<V>::RequestGetRowRPC(int32_t _table_id, int32_t _row_id, 
    Row<V> *_row, int32_t _stalest_iter){
  VLOG(1) << "RequestGetRowRPC called";
  SCGetRowMsg msg(pthread_self(), _table_id, _row_id, _stalest_iter);

  TablePartitioner& partitioner = TablePartitioner::GetInstance();
  int32_t server_id = partitioner.GetRowAssignment(_table_id, _row_id);
  VLOG(3) << "Requesting table " << _table_id 
	  << " row " << _row_id 
	  << " from server " << server_id;
  int suc = comm_.Send(server_id, (uint8_t *) &msg,
      sizeof(SCGetRowMsg));
  assert(suc == sizeof(SCGetRowMsg));
  int32_t sid;
  boost::shared_array<uint8_t> data;
  bool more;

  VLOG(2) << "RequestGetRowRPC starts waiting for response";
  suc = comm_.RecvThr(sid, data, &more);
  VLOG(2) << "RequestGetRowRPC, comm_.RecvThr, suc = " << suc;
  assert(suc == sizeof(SCGetRowReplyMsg));
  SCGetRowReplyMsg *reply = (SCGetRowReplyMsg *) data.get();
  if(reply->ret_ < 0) return -1;
  assert(more);
  // the return value is the number of bytes in data
  suc = comm_.RecvThr(data);
  VLOG(2) << "received row data, size = " << suc;
  assert(suc > 0);
  _row->Deserialize(data, suc);
  return 0;
}

template<typename V>
int ClientProxy<V>::SendOpLogs(int32_t _table_id, int32_t _server_id,
    const boost::shared_array<uint8_t> &_op_log, int32_t _size) {
  SCPushOpLogMsg msg(_table_id);
  // TODO(jinliang): Right now we send the OpLog of the entrie
  // table to all servers. Actually, each row in OpLog table and
  // each row in data storage should keep its assignment information 
  // and OpLog should be serialized based on its OpLog assignment.
  // Second option is broadcast.
  // In first option, should consider the problem of dynamic row 
  // reassignment -- let server forward the OPLog to the correct location.
  int suc = comm_.Send(_server_id, (uint8_t *) &msg, sizeof(SCPushOpLogMsg), 
      true, true);
  assert(suc == sizeof(SCPushOpLogMsg));
  suc = comm_.Send(_server_id, _op_log.get(), _size, false, false);
  assert(suc == _size);
  return 0;
}

template<typename V>
int ClientProxy<V>::SendIterate(int32_t _table_id, int32_t _server_id,
    const boost::shared_array<uint8_t> &_op_log, int32_t _size) {
  SCSendIterateMsg msg(_table_id);
  // TODO(jinliang): Right now we send the OpLog of the entrie
  // table to all servers. Actually, each row in OpLog table and
  // each row in data storage should keep its assignment information 
  // and OpLog should be serialized based on its OpLog assignment.
  // Second option is broadcast.
  // In first option, should consider the problem of dynamic row 
  // reassignment -- let server forward the OPLog to the correct location.
  int suc = comm_.Send(_server_id, (uint8_t *) &msg, sizeof(SCSendIterateMsg), 
      true, true);
  assert(suc == sizeof(SCSendIterateMsg));
  suc = comm_.Send(_server_id, _op_log.get(), _size, false, false);
  assert(suc == _size);
  return 0;

}

/*=============== Private Functions ==============*/
// currently, the first id in server list is used as name node
template<typename V>
int32_t ClientProxy<V>::InitNameNode(){
  if (servers_.size() == 0) {
    LOG(ERROR) << "servers_ size cannot be 0.";
  }
  return servers_[0].id_;
}

}   // namespace petuum


#endif
