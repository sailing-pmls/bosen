#ifndef __PETUUM_SERVER_MT__
#define __PETUUM_SERVER_MT__

#include "petuum_ps/include/configs.hpp"
#include "petuum_ps/storage/concurrent_row_storage.hpp"
#include "petuum_ps/consistency/op_log_manager.hpp"
#include "petuum_ps/comm_handler/comm_handler.hpp"
#include "petuum_ps/util/vector_clock.hpp"
#include "petuum_ps/storage/dense_row.hpp"

#include <vector>
#include <pthread.h>
#include <glog/logging.h>
#include <boost/thread.hpp>
#include <boost/foreach.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include <boost/scoped_ptr.hpp>
#include <tbb/concurrent_unordered_map.h>
#include <tbb/concurrent_hash_map.h>


namespace petuum {

enum ServerRetType{BroadcastServersReply, ReplySender, DropMsg, SendMsgs};

/*
 * When ServerProxy receives a request, the message is decoded and ServerProxy
 * invokes the corresponding function of Server to perform the action. When 
 * the action is done, Server return a ServerRet type to the caller, which 
 * instructs the ServerProxy to make perform further actions. Those actions 
 * are general actions not message specific actions (not specific to 
 * CreateTable, GetRow and so on). In this way, server logic is completely 
 * contained in Server.Server is responsible for ensuring the action can be 
 * performed. If a particular action cannot be performed, the message will 
 * simply be droped.
 * 
 * BroadcastServersReply:
 * Broadcast the same request to all other servers, wait for other servers' 
 * responses, then reply the sender of the message if all other servers agree 
 * on the same return value. Otherwise, abort the process.When Server 
 * (NameNode) asks for Broadcast, it means the action is already successfully
 * performed on the name node. The name node then asks other server to get to
 * the same page with himself, so he expects the same return value from other
 * servers. If other server cannot return the same value, something is wrong.
 * Since we cannot handle this fault yet (it may be code bug, or could be 
 * runtime problem), there's nothing better to do than aborting.
 * Note that this only takes effect if the server itself is a name node, since
 *  non-name-node server has no knowledge about other servers. So if the 
 * return action is received at a non-name-node server, it is perceived as 
 * DropMsg.
 *
 * ReplySender:
 * Reply sender(could be anothr server or a client) with the return value.
 * 
 * DropMsg:
 * Action is successfully performed, no need to take further action, so the 
 * message can be droped.
 *
 * 
 */
struct ServerRetMsg {

  ServerRetType type_;
  int ret_;
};

struct ServerTableMeta {

  VectorClock clock_;
  TableConfig config_;

  ServerTableMeta() { }

  ServerTableMeta(const std::vector<int32_t> client_ids, TableConfig config):
    clock_(client_ids), config_(config) { }
};

struct ReadRequest {
  int32_t client_id_;
  pthread_t thread_id_;

  ReadRequest(int32_t _client_id, pthread_t _thread_id):
    client_id_(_client_id),
    thread_id_(_thread_id){}
};

struct RowReadReply {
  int32_t row_id_;
  std::vector<ReadRequest> requests_;
  boost::shared_array<uint8_t> data_;
  int32_t num_bytes_;
};

template<typename V>
using DenseConcurrentRowStorage = ConcurrentRowStorage<DenseRow, V>;

template<typename V>
class Server {
  public:
    explicit Server();
    virtual ~Server();

    int Init(const std::vector<int32_t> &client_ids,
        bool is_namenode);
    // ========================= Table operations =========================

    // CreateTableNameNode use 'config' to create table if table_id is not
    // already initialized. If table_id already exists, then write the existing
    // config to 'config'. This method can only be called by namenode.
    virtual ServerRetMsg CreateTableNameNode(int32_t table_id,
        TableConfig* config);

    // CreateTableNonNameNode can only be called once on a non-namenode.
    virtual ServerRetMsg CreateTableNonNameNode(int32_t table_id,
        TableConfig* config);

    virtual ServerRetMsg Iterate(int32_t client_id, int32_t table_id,
        const std::vector<EntryOpExtended<V> >& op_log,
        std::vector<RowReadReply>* reply);

    virtual ServerRetMsg ApplyOpLogs(int32_t table_id,
        const std::vector<EntryOpExtended<V> >& op_log);

    // ========================= Row operations =========================

    virtual ServerRetMsg GetRow(int32_t client_id, pthread_t thread_id, 
        int32_t table_id, int32_t row_id, 
        boost::shared_array<uint8_t> &row_data,
        int32_t &num_bytes,
        int32_t row_iter = 0);

    // ========================= Test functions =========================

    // Only for test.
    virtual int get_num_clients() const;

  private:

    void ApplyOpLogsInternal(int32_t table_id, const TableConfig &table_config,
        const std::vector<EntryOpExtended<V> >& op_log);

    int GetFulfilledReads(int32_t table_id, std::vector<RowReadReply>* reply, 
        int iter);
    /*
     * Table_directory maps table id to table meta data, which contains clock and 
     * table config.
     * The table allows concurrent reads (GetRow) but not concurent writes (apply 
     * oplogs).
     * TODO (jinliang): do we need concurrent writes?
     * 
     * Any access to that table must acquire lock to that table by getting the 
     * appropriate accessor.We only allow exclusive access fo writer (apply 
     * oplogs) because we need to guarantee that clients only reads data at 
     * iteration boundary.
     * 
     */
    tbb::concurrent_hash_map<int32_t, ServerTableMeta> table_directory_;
    tbb::concurrent_unordered_map<int32_t, DenseConcurrentRowStorage<V> >
      dense_storage_directory_;

    // mapping row id to read requests
    typedef boost::unordered_map<int32_t, 
            std::vector<ReadRequest> > TableReads;
    // mapping iter number to read requests, reads pending for a particular 
    // iteration
    typedef boost::unordered_map<int, TableReads> IterReads;
    // mapping table id to pending reads
    typedef boost::unordered_map<int32_t, IterReads> PendingReads;
    PendingReads pending_reads_;

    boost::mutex pending_read_mutex_; // mutex for pending_read_

    // assume std::vector supports concurrent reads
    // GCC's standard can be found here: 
    // http://gcc.gnu.org/onlinedocs/libstdc++/manual/using_concurrency.html
    // suuports concurrent reads but exclusive writes
    std::vector<int32_t> client_ids_;
    bool is_namenode_;
};

// ======================== Public Methods ======================== 

template<typename V>
Server<V>::Server(){}

template<typename V>
Server<V>::~Server(){}

template<typename V>
int Server<V>::Init(const std::vector<int32_t> &client_ids, bool is_namenode){
  client_ids_ = client_ids; 
  is_namenode_ = is_namenode;
  return 0;
}

template<typename V>
ServerRetMsg Server<V>::CreateTableNameNode(int32_t table_id, 
    TableConfig* config)
{
  CHECK(is_namenode_)
    << "CreateTableNameNode should only be called for namenode.";
  ServerRetMsg ret_msg;
  // TODO(jinliang, wei dai): should check if table is dense
  // or sparse
  {
    // Acquire a read lock to check if the table already exists,
    // Return if found.
    tbb::concurrent_hash_map<int32_t, ServerTableMeta>::const_accessor
      table_acc;
    if (table_directory_.find(table_acc, table_id)) {
      // Received 2 or more CreateTable request.
      *config = (table_acc->second).config_;
      ret_msg.type_ = ReplySender;
      ret_msg.ret_ = 1;
      return ret_msg;
    }
  }

  // Table not found, acquire wirte lock to create the table.
  tbb::concurrent_hash_map<int32_t, ServerTableMeta>::accessor
    table_acc;
  if (!table_directory_.insert(table_acc, table_id)) {
    // Another thread has already inserted the table when table_acc was
    // released earlier.
    *config = (table_acc->second).config_;
    ret_msg.type_ = ReplySender;
    ret_msg.ret_ = 1;
    return ret_msg;
  }

  // Creating table with table_acc lock.
  ServerTableMeta table_meta(client_ids_, *config);
  table_acc->second = table_meta;
  DenseConcurrentRowStorage<V> new_table(config->server_storage_config);
  dense_storage_directory_.insert(std::make_pair(table_id, new_table));

  // Return 0 for success and broadcast to create table on all server nodes.
  ret_msg.type_ = BroadcastServersReply;
  ret_msg.ret_ = 0;
  return ret_msg;
}

template<typename V>
ServerRetMsg Server<V>::CreateTableNonNameNode(int32_t table_id,
    TableConfig* config) {
  CHECK(!is_namenode_)
    << "CreateTableNameNode should only be called for non-namenode.";
  {
    // Acquire a read lock to check if the table already exists,
    // Return if found.
    tbb::concurrent_hash_map<int32_t, ServerTableMeta>::const_accessor
      table_acc;
    CHECK(!table_directory_.find(table_acc, table_id))
      << "Attempting to create a table twice on a non-namenode. Report bug.";
  }

  // Acquire write lock.
  tbb::concurrent_hash_map<int32_t, ServerTableMeta>::accessor
    table_acc;
  CHECK(table_directory_.insert(table_acc, table_id))
    << "Attempting to create a table twice on a non-namenode. Report bug.";

  // Creating table with table_acc lock.
  ServerTableMeta table_meta(client_ids_, *config);
  table_acc->second = table_meta;
  DenseConcurrentRowStorage<V> new_table(config->server_storage_config);
  dense_storage_directory_.insert(std::make_pair(table_id, new_table));

  // Reply 0 for success.
  ServerRetMsg ret_msg;
  ret_msg.type_ = ReplySender;
  ret_msg.ret_ = 0;
  return ret_msg;
}

template<typename V>
ServerRetMsg Server<V>::Iterate(int32_t client_id, int32_t table_id,
    const std::vector<EntryOpExtended<V> >& op_log,
    std::vector<RowReadReply>* reply)
{
  CHECK_NOTNULL(reply);
  ServerRetMsg ret_msg;
  tbb::concurrent_hash_map<int32_t, ServerTableMeta>::accessor
    table_acc;

  CHECK(table_directory_.find(table_acc, table_id))
    << "Table id = " << table_id << " not found!!";

  ApplyOpLogsInternal(table_id, table_acc->second.config_, op_log);

  int itr = (table_acc->second).clock_.Tick(client_id);

  if (itr > 0) {
    ret_msg.type_ = SendMsgs;
    ret_msg.ret_ = GetFulfilledReads(table_id, reply, itr);
    VLOG(0) << "Reply GetRow!";
  } else {
    // client_id isn't the slowest one and did not advance server clock.
    ret_msg.type_ = DropMsg;
    ret_msg.ret_ = 0;
  }

  return ret_msg;
}

template<typename V>
void Server<V>::ApplyOpLogsInternal(int32_t table_id,
    const TableConfig &table_config,
    const std::vector<EntryOpExtended<V> >& op_log)
{

  /* TODO: Need to decide whether table is in desen storage or sparse 
     storage */

  VLOG(3) << "ApplyOpLogsInternal...";
  if (VLOG_IS_ON(3)) {
    for (int i = 0; i < op_log.size(); ++i) {
      VLOG(3) << "(row_id, col_id) = (" << op_log[i].row_id << ", " <<
        op_log[i].col_id << "). Op="
        << (op_log[i].op == EntryOp<V>::PUT ? "PUT" : "INC") << " Val="
        << op_log[i].val;
    }
  }
  typename std::vector<EntryOpExtended<V> >::const_iterator it;
  for (it = op_log.begin(); it != op_log.end(); it++) {
    switch (it->op) {
      case EntryOpExtended<V>::INC:
        if (!dense_storage_directory_[table_id].Inc(it->row_id, it->col_id, 
              it->val)) {
          // Row doesn't exist.
          int num_columns = table_config.num_columns;
          DenseRow<V> new_row(num_columns);
          dense_storage_directory_[table_id].PutRow(it->row_id, new_row);
          int ret = dense_storage_directory_[table_id].Inc(it->row_id, 
              it->col_id, 
              it->val);
          assert(ret);
        }
        break;
      case EntryOpExtended<V>::PUT:
        if (!dense_storage_directory_[table_id].Put(it->row_id, it->col_id, 
              it->val)) {
          // Row doesn't exist.
          int num_columns = table_config.num_columns;
          DenseRow<V> new_row(num_columns);
          dense_storage_directory_[table_id].PutRow(it->row_id, new_row);
          int ret = dense_storage_directory_[table_id].Put(it->row_id, 
              it->col_id, 
              it->val);
          assert(ret);
        }
        break;
      default:
        assert(0);
    }
  }
  if (VLOG_IS_ON(3)) {
    V val;
    dense_storage_directory_[table_id].Get(0, 0, &val);
    VLOG(3) << "Get after oplog: (row_id, col_id, val) = (0, 0, " << val << ")";
  }
}

template<typename V>
ServerRetMsg Server<V>::ApplyOpLogs(int32_t table_id,
    const std::vector<EntryOpExtended<V> >& 
    op_log){
  ServerRetMsg ret_msg;
  tbb::concurrent_hash_map<int32_t, ServerTableMeta>::accessor
    table_acc;
  CHECK(table_directory_.find(table_acc, table_id))
    << "Table id = " << table_id << " not found!!";

  ApplyOpLogsInternal(table_id, table_acc->second.config_, op_log);
  ret_msg.type_ = DropMsg;
  return ret_msg;
}

template<typename V>
ServerRetMsg Server<V>::GetRow(int32_t client_id, pthread_t thread_id,
    int32_t table_id, int32_t row_id, 
    boost::shared_array<uint8_t> &row_data,
    int32_t &num_bytes,
    int32_t row_iter)
{
  ServerRetMsg ret_msg;
  tbb::concurrent_hash_map<int32_t, ServerTableMeta>::const_accessor
    table_acc;
  CHECK(table_directory_.find(table_acc, table_id))
    << "Table id = " << table_id << " not found!!";

  int client_clock = (table_acc->second).clock_.get_client_clock(client_id);
  int server_clock = (table_acc->second).clock_.get_slowest_clock();

  if (server_clock >= row_iter) { 
    // fresh enough, respond immediately
    DenseRow<V> row_val;
    int ret = dense_storage_directory_[table_id].GetRow(row_id, &row_val, 0);
    if (ret == 0) {
      // Row doesn't exist.	
      int num_columns = (table_acc->second).config_.num_columns;
      DenseRow<V> new_row(num_columns);
      dense_storage_directory_[table_id].PutRow(row_id, new_row);
      ret = dense_storage_directory_[table_id].GetRow(row_id, &row_val, 0);
      assert(ret > 0);
    }
    // Reply client with row_val and success
    row_val.set_iteration(server_clock);
    VLOG(1) << "GetRow returns row " << row_id << ". Column 0 = " << row_val[0]
      << " with iteration " << server_clock;
    num_bytes = row_val.Serialize(row_data);
    ret_msg.type_ = ReplySender;
    ret_msg.ret_ = 0;
  } else {
    // This client is too fast. We block the read and queue the request
    boost::unique_lock<boost::mutex> write_lock(pending_read_mutex_);
    pending_reads_[table_id][row_iter][row_id].push_back(
        ReadRequest(client_id, 
          thread_id));
    ret_msg.type_ = DropMsg;
    ret_msg.ret_ = 0;
  } // end of if check readability
  return ret_msg;
}

template<typename V>
int Server<V>::get_num_clients() const
{
  return client_ids_.size();
}


// ======================== Private Methods ======================== 


// read lock on the table must be held
template<typename V>
int Server<V>::GetFulfilledReads(int32_t table_id, 
    std::vector<RowReadReply>* reply, int iter){
  boost::unique_lock<boost::mutex> write_lock(pending_read_mutex_);

  PendingReads::iterator table_iter = pending_reads_.find(table_id);

  if(table_iter == pending_reads_.end()) return 0;

  IterReads::iterator iter_iter = (table_iter->second).find(iter);

  if(iter_iter == (table_iter->second).end()) return 0;

  TableReads::iterator row_iter = (iter_iter->second).begin();

  int num_pending = 0;
  for(; row_iter != (iter_iter->second).end(); ++row_iter){
    DenseRow<V> row;
    boost::shared_array<uint8_t> row_data;
    // use 0 to bypass the staleness checker in GetRow
    // TODO(Wei Dai): remove staleness checker from RowStorage
    int ret = dense_storage_directory_[table_id].GetRow(row_iter->first, 
        &row, 0);
    CHECK_EQ(ret, 1) << "GetRow failed" 
      << " table " << table_id
      << " row " << row_iter->first;

    row.set_iteration(iter); // need to set iteration number before return

    int32_t size = row.Serialize(row_data);
    RowReadReply row_reply;
    row_reply.row_id_ = row_iter->first;
    row_reply.requests_ = row_iter->second;
    row_reply.data_ = row_data;
    row_reply.num_bytes_ = size;
    reply->push_back(row_reply);
    ++num_pending;
  }
  return num_pending;
}


}  // namespace petuum

#endif // __PETUUM_SERVER_MT__
