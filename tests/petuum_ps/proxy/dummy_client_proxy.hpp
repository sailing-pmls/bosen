// Copyright (c) 2014, Sailing Lab
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
#ifndef PETUUM_DUMMY_CLIENT_PROXY
#define PETUUM_DUMMY_CLIENT_PROXY

#include "petuum_ps/proxy/client_proxy.hpp"
#include "petuum_ps/storage/row_interface.hpp"
#include "tests/petuum_ps/comm_handler/dummy_comm_handler.hpp"
#include <boost/shared_array.hpp>
#include <glog/logging.h>

namespace petuum {

namespace {

DummyCommHandler dummy_comm_handler;
ClientProxyConfig client_proxy_config;

}   // anonymous namespace

template<typename V>
class DummyClientProxy : public ClientProxy<V> {
  public:
    // Easy to use default constructor
    DummyClientProxy();

    virtual int RequestGetRowRPC(int32_t table_id, int32_t row_id, Row<V> *row,
        int32_t stalest_row_iter);

    virtual int SendIterate(int32_t _table_id,
          const boost::shared_array<uint8_t> &_op_log,
          int32_t _size);

    virtual int SendOpLogs(int32_t _table_id,
			   const boost::shared_array<uint8_t> &_op_log,
			   int32_t _size);
};

template<typename V>
DummyClientProxy<V>::DummyClientProxy()
  : ClientProxy<V>(dummy_comm_handler, client_proxy_config) {
    LOG(INFO) << "DummyClientProxy created.";
  }

template<typename V>
int DummyClientProxy<V>::RequestGetRowRPC(int32_t table_id, int32_t row_id,
    Row<V> *row, int32_t stalest_row_iter) {
  // Just create row of 3 columns.
  LOG(INFO) << "Got RequestGetRowRPC for row " << row_id << ", stalest_row_iter = "
    << stalest_row_iter;
  DenseRow<V> row_data(3);
  row_data[0] = 1;
  row_data[1] = 2;
  row_data[2] = 3;
  row_data.set_iteration(999);  // always fresh!

  // Set row by deserializing.
  boost::shared_array<uint8_t> row_bytes;
  int32_t num_bytes = row_data.Serialize(row_bytes);
  CHECK_EQ(0, row->Deserialize(row_bytes, num_bytes));
  return 0;
}

template<typename V>
int DummyClientProxy<V>::SendIterate(int32_t _table_id,
			  const boost::shared_array<uint8_t> &_op_log, int32_t _size) {
  LOG(INFO) << "Got SendIterate from table " << _table_id << " op log has "
    << _size << " bytes.";
  return 0;
}

template<typename V>
int DummyClientProxy<V>::SendOpLogs(int32_t _table_id,
			  const boost::shared_array<uint8_t> &_op_log, int32_t _size) {
  LOG(INFO) << "Got SendOpLogs from table " << _table_id << " op log has "
    << _size << " bytes.";
  return 0;
}

}  // namespace petuum

#endif  // PETUUM_DUMMY_CLIENT_PROXY
