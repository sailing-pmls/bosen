
#ifndef __PETUUM_ARCHITECTURE_HPP__
#define __PETUUM_ARCHITECTURE_HPP__

#include <stdint.h>
#include <boost/scoped_ptr.hpp>
#include <tbb/concurrent_unordered_map.h>

namespace petuum{

  // Internal low-level data structures are defined below but they are referenced by the external user interfaces
  template<typename RowID>
  class ConsistencyController;

  template<typename RowID>
  class OpLogTable;

  /******************* External user interface ***********************************************/
  // note that we only have external user interface on client side
  // there's no external user interface on server side
  /******************* Table User Interface - always functional, same interface for all users ***************************/

  template<typename ColID, typename ValueT>
  class RowBatch{
    // used for batched (atomic) row operations
  };

  // on Table level, user need to decide key/value size (if they are fixed) see HashTable below for more details
  template<typename RowID, typename ColID, typename ValueT>
  class Table{
  private:
    ConsistencyController<RowID> controller_;
  public:

    int Get(RowID _rid, ColID _cid, ValueT *_value);
    // interface for put, inc, iterate...
    // for each Get, Put..., call corresponding DoXXX function of controller_

    int CreateRow(/*...*/);

    // batched (atomic) row operations
    int GetRowBatch(RowID _rid);
    int ApplyRowBatch(/*arguments*/);
  };


  // Highest-level user handler, a singleton
  // TableID, RowID, ColID, ValueT all should be PODs or primitive types
  // TableID is only used in TableGrou; it is not propagated to other classes
  // ColID, ValueT are only used by user interfaces, they are all treated as byte-array internally
  template<typename TableID, typename RowID, typename ColID, typename ValueT>
  class TableGroup{
  private:
    tbb::concurrent_unordered_map<TableID, Table<RowID, ColID, ValueT> > tables_;
  public:
    // _synchronous determines if the table's parallel execution model is synchronous or asynchronous
    // The difference is that in synchronous mode Iterate() on table is functional (increment iteration number by 1 and do communication...)
    // In asynchronous mode, Iterate() is a no-op
    // Note that we allow synchronous table and asynchronous tables to coexist within the same table group to all semi-synchronous execution
    // (haven't thought it through, but seems it would work; also it is questionable if semi-synchronous is really useful)
    // semi-synchrnous execution: clients run the same number of iterations (super-steps); within each super-step, the clients are divided into subsets,
    // within each subset, clients run asynchronously; global synchronization is done in the end of each super-step
    Table<RowID, ColID, ValueT> CreateTable(TableID _tid, bool _synchronous/* other parameters */ );

    Table<RowID, ColID, ValueT> GetTable(TableID _tid,);
  };


  /************************ Consistency control interface - optional ************************************************************/
  // note: users should be able to use full table interface (above) without doing anything regarding this consistency interface

  enum EActionType {PUSH, PULL /*othter actions, like "discard previous inc" ...*/};

  // user may subclass this class to make use of different policy
  template<typename RowID, typename ColID, typename ValueT>
  class ConsistencyControlPolicy {
  private:
    ConsistencyController<RowID> controller_;
  public:
    EActionType CheckGet(RowID _rid /* other params */);
    // CheckPut, CheckInc ...
  };

  // tentatively we have one consistency controller per Table, thus the consistency controller only interacts with *one* TabletStorage and *one* OpLogTable
  // The benefit is that 1) we do not have to look up TabletStorage and OpLogTable within the controller
  //                     2) we keep the template type TableID within TableGroup
  // The downside is probably higher memory footprint
  template<typename RowID>
  class ConsistencyController {
  private:
    TabletStorage<RowID> table_;
    OpLogTable<RowID> oplog_table_;
  public:
    int DoGet(/* params */);
    int DoPut(/* params */);
    // other DoXXX

    // interface for ConsistencyControlPolicy to access table_ data and operation log data
    // need to think more about that

    // each DoXXX will invoke the corresponding check function of ConsistencyControllPolicy
    // depending on the returned Action, the controller will perform different actions
  };

  /******************* Internal interfaces, invisible to users ***************************/

  enum EOpType {PUT, INC, GET, ITERATE};

  class OpLog {
    EOpType op_;
    uint8_t *v_;
    int32_t vsize_;
  };

  template<typename RowID>
  class OpLogTable {
  };

  // Tentatively, I'm thinking of using one dedicated thread to manange OpLogTable (listening to server responses and make modifications to OpLogTable...
  // could be changed
  class OpLogManager {

  };

  // representing in a row in parameter server
  // row is the unit of consistency control
  class TabletRow {
  private:
    uint32_t iteration_; // used only in synchronous mode
    ConsistencyControlPolicy &policy_;
    HashTable row_data_; // mapping from column id to row id
  };


  template<typename RowID>
  class TabletStorage{
    tbb::concurrent_unordered_map<RowID, TabletRow> rows_;
    // support Put, Get, Inc...

    int CreateRow(RowID _rid);
  };

  // The interface is general hash table interface
  // Our implementation will be Cuckoo hashing
  // Mapping fixed-sized byte array to fixed-sized byte array

  // The interface is kind of ugly. We pay the price to have this HashTable to support both fixed-sized keys (values) and variable-sized keys (values).

  // TODO(Jinliang): the interface is preliminary, will be optimized later
  class HashTable {
  public:
    // user need to specify the size of keys and values for fixed-sized keys and values
    // if _key_size <= 0, keys and values are of variable size
    HashTable(int32_t _key_size, int32_t _value_size);
    int Put(const uint8_t *_key, const int8_t *_value);
    int Put(const uint8_t *_key, int32_t _ksize, const int8_t *_value, int32_t _vsize);

    int Get(const uint8_t *_key, uint8_t *_value);
    int Get(const uint8_t *_key, int32_t _ksize, boost::scoped_ptr<uint8_t> *_value, int32_t _vsize);

    // interface for inc
    // interface for serialize
    // interface for traversal

  };

}

#endif
