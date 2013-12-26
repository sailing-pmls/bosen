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

#ifndef __PETUUM_INCLUDE_TABLE_HPP__
#define __PETUUM_INCLUDE_TABLE_HPP__

#include "petuum_ps/include/table_row.hpp"
#include "petuum_ps/storage/dense_row.hpp"
#include "petuum_ps/storage/sparse_row.hpp"
#include "petuum_ps/proxy/client_proxy.hpp"
#include "petuum_ps/consistency/consistency_policy.hpp"
#include "petuum_ps/consistency/consistency_controller.hpp"
#include "petuum_ps/proxy/table_partitioner.hpp"
#include <petuum_ps/stats/stats.hpp>
#include <sstream>
#include <boost/utility.hpp>

namespace petuum {

// Forward declaration to break dependency.
template <typename V>
class TableGroup;

template<template<typename> class ROW, typename V>
class Table;

// Define user-facing tables.
template<typename V>
using DenseTable = Table<DenseRow, V>;

// Define user-facing tables.
template<typename V>
using SparseTable = Table<SparseRow, V>;

// Table delegates table operations to ConsistencyController, which handles
// the necessary synchronizations according to the ConsistencyPolicy.
// Furthermore, The actual storage is maintained internally in
// ConsistencyController and is completely hidden from Table.
//
// Comment(wdai): Table is thread-safe assuming ConsistencyController is
// thread-safe (which it is).
template<template<typename> class ROW, typename V>
class Table : boost::noncopyable {
  public:
    // ======== Entry-wise Table Operations =======
    //
    // Note that in all these operations if row row_id has not been
    // initialized, then it'll be initialized both on the server and cached in
    // processor and thread cache.  For each of the operation
    // ConsistencyController will check against policy_ and perform necessary
    // synchronizations.

    // If no value has been Put() or Inc(), then Get() returns default value 0.
    virtual V Get(int32_t row_id, int32_t col_id);

    // Get row reference, which could be invalid if another Get() or
    // GetRowUnsafe() is called.
    virtual ROW<V>& GetRowUnsafe(int32_t row_id);

    // Overwrite any existing value locally. On the server side it's last
    // writer wins.
    virtual void Put(int32_t row_id, int32_t col_id, V new_val);

    // Inc assumes the "+=" operator of V is defined. It should be a
    // commutative and associative (i.e., abelian) operation.
    virtual void Inc(int32_t row_id, int32_t col_id, V delta);

    // Comment(wdai): Comment out or provide fake implement of the Row-level
    // functions.
    //
    // When access multiple values in a row, it is suggested to call GetRow()
    // instead of calling Get() multiple times.  The reference actually
    // references to a data structure stored in thread cache. That means that
    // row cannot be evicted when it is referenced.  We need to maintain a
    // reference counter in that row.
    //ROW<V> GetRow(int32_t _rid);

    //RowBatch<V> GetRowBatch();

    //int UpdateRow(int32_t _rid, const RowBatch<V> _batch);

    friend class TableGroup<V>;

    friend class TableTest;   // For testing purpose.

  private:   // private functions
    // Disable default constructor
    Table();

    // Table constructor assumes that TableGroup has already contacted servers
    // to create server table partitions, and the constructor here merely
    // initialize process level table. Only TableGroup can access this
    // constructor.
    explicit Table(int32_t table_id, const TableConfig& _table_config,
		   ClientProxy<V>& client_proxy, 
		   boost::thread_specific_ptr<StatsObj> &thr_stats);

    // Comment(wdai): We hide Iterate() at Table level to force user to use
    // Iterate() in TableGroup.
    void Iterate();

    // Register thread forwards the call to controller_.
    void RegisterThread();

  private:
    // All table read and write will go through controller_ to ensure
    // consistency. controller_ will create the storage class internally.
  ConsistencyController<ROW, V> controller_;
  
  boost::scoped_ptr<SspPolicy<V> > ssp_policy_;
  
  boost::thread_specific_ptr<StatsObj> &thr_stats_;
  std::string table_id_str; // just to facilitate StatsObj
};

// ==================== Table Implementation ====================

template<template<typename> class ROW, typename V>
Table<ROW, V>::Table(int32_t table_id, const TableConfig& table_config,
		     ClientProxy<V>& client_proxy, 
		     boost::thread_specific_ptr<StatsObj> &thr_stats):
  thr_stats_(thr_stats),
  controller_(thr_stats){

  // Construct SSP
  ssp_policy_.reset(new SspPolicy<V>(table_config.table_staleness));

  // Create ConsistencyControllerConfig.
  ConsistencyControllerConfig<V> controller_config;
  controller_config.table_id = table_id;
  controller_config.num_threads = table_config.num_threads;
  controller_config.num_servers = table_config.num_servers;
  controller_config.storage_config = table_config.process_storage_config;
  controller_config.policy = ssp_policy_.get();
  controller_config.proxy = &client_proxy;

  // Inject necessary parameters to op_log_config.
  controller_config.op_log_config = table_config.op_log_config;
  controller_config.op_log_config.num_servers = table_config.num_servers;
  controller_config.op_log_config.table_id = table_id;

  // Initialize table partition (currently all tables have the same partition,
  // so this is redudant).
  TablePartitioner& partitioner = TablePartitioner::GetInstance();
  partitioner.Init(table_config.num_servers);

  // Initialize ConsistencyController
  controller_.Init(controller_config);
  VLOG(0) << "Table " << table_id << " created with staleness "
    << table_config.table_staleness;

  std::stringstream ss;
  ss << table_id;
  table_id_str = std::string("Table.") + ss.str();
}

template<template<typename> class ROW, typename V>
V Table<ROW, V>::Get(int32_t row_id, int32_t col_id) {

  V val;
  CHECK_EQ(0, controller_.DoGet(row_id, col_id, &val));
  VLOG(2) << "Get row " << row_id << " col " << col_id << " with val " << val;

  return val;
}

template<template<typename> class ROW, typename V>
ROW<V>& Table<ROW, V>::GetRowUnsafe(int32_t row_id) {
  return controller_.DoGetRowUnsafe(row_id);
}

template<template<typename> class ROW, typename V>
void Table<ROW, V>::Put(int32_t row_id, int32_t col_id, V new_val) {
  CHECK_EQ(0, controller_.DoPut(row_id, col_id, new_val));

}

template<template<typename> class ROW, typename V>
void Table<ROW, V>::Inc(int32_t row_id, int32_t col_id, V delta) {

  CHECK_EQ(0, controller_.DoInc(row_id, col_id, delta));
}

template<template<typename> class ROW, typename V>
void Table<ROW, V>::Iterate() {
  CHECK_EQ(0, controller_.DoIterate());
}

template<template<typename> class ROW, typename V>
void Table<ROW, V>::RegisterThread() {

  controller_.RegisterThread();
}

}  // namespace petuum

#endif  // __PETUUM_TABLE_HPP__
