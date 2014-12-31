#ifndef PETUUM_CONSISTENCY_CONTROLLER
#define PETUUM_CONSISTENCY_CONTROLLER

#include "petuum_ps/include/configs.hpp"
#include "petuum_ps/consistency/consistency_policy.hpp"
#include "petuum_ps/consistency/op_log_manager.hpp"
#include "petuum_ps/util/vector_clock.hpp"
#include "petuum_ps/storage/thread_safe_lru_row_storage.hpp"
#include "petuum_ps/proxy/client_proxy.hpp"
#include "petuum_ps/stats/stats.hpp"

#include <atomic>
#include <glog/logging.h>
#include <boost/utility.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/thread/tss.hpp>
#include <boost/thread.hpp>
#include <boost/thread/barrier.hpp>

namespace petuum {

template<typename V>
class ConsistencyPolicy;

template<typename V>
class ClientProxy;

// Constructor parameters for ConsistencyController.
template<typename V>
struct ConsistencyControllerConfig {

  StorageConfig storage_config;

  // ConsistencyController synchronizes based on policy.
  ConsistencyPolicy<V>* policy;

  // proxy is the only means ConsistencyController talks to servers.
  ClientProxy<V>* proxy;

  OpLogManagerConfig op_log_config;
};

// ThreadTableInfo stores thread-specific information of a table, including
// OpLogManager , to provide a thread view of the table.
template<template<typename> class ROW, typename V>
class ThreadTableInfo {
  public:
    ThreadTableInfo(int32_t thread_id,
		    const OpLogManagerConfig& op_log_config,
		    StatsObj &thr_stats, int32_t table_id) :
      thread_id_(thread_id), iter_(0), thr_stats_(thr_stats),
      op_log_manager_(op_log_config, thr_stats, table_id),
      table_id_(table_id){
        VLOG(3) << "ThreadTableInfo created for thread_id = " << thread_id;
      }

    inline int32_t GetThreadIter() const {
      return iter_;
    }

    // Increment iteration number iter_.
    inline void IncrementThreadIter() {
      iter_++;
    }

    inline int32_t get_thread_id() const {
      return thread_id_;
    }

    inline OpLogManager<ROW, V>& GetOpLogManager() {
      return op_log_manager_;
    }

  private:
    // Iteration number.
    int32_t iter_;

    // thread_id_ is used in clock manager.
    int32_t thread_id_;

    StatsObj &thr_stats_;

    // OpLog associated with a particular table.
    OpLogManager<ROW, V> op_log_manager_;

  int32_t table_id_;
};


// ConsistencyController handles all table operations from application to a
// table and maintain consistency based on consistency policies defined in
// ConsistencyPolicy. To maintain consistency, ConsistencyController maintains
// a pointer to CommHandler to sync with the tablet servers when needed. Note
// that there is one ConsistencyController associated with each table at the
// processor-level. The class is noncopyable.
//
// Comment(wdai): ConsistencyController requires the underlying storage
// (ThreadSafeTabletStorage) to be thread-safe. Implement thread-safety at
// lower-level allows higher concurrency when we implement concurrent Cuckoo
// hash table.  It also release the burden of maintaining mutex within
// ConsistencyController, which already has a lot of responsibilities.  Note
// with the assumption that storage is thread-safe, all methods in
// ConsistencyController will need to be thread-safe as well.
template<template<typename> class ROW, typename V>
class ConsistencyController : boost::noncopyable {
public:
  // Default constructor requires a Init() call.
  ConsistencyController(boost::thread_specific_ptr<StatsObj> &thr_stats);
  
  void Init(const ConsistencyControllerConfig<V>& controller_config);

  // TODO(wdai) : Use decision_info_(*this) in initialization list.
  explicit ConsistencyController(
				 const ConsistencyControllerConfig<V>& controller_config,
				 boost::thread_specific_ptr<StatsObj> &thr_stats);

    // Read an entry in the table. The implementation should check whether to
    // pull with pull_policy_. The implementation should be thread-safe.
    // Return 0 on success, negatives for error code.
    inline int DoGet(int32_t row_id, int32_t col_id, V* val);

    // Unsafe GetRow.
    inline ROW<V>& DoGetRowUnsafe(int32_t row_id);

    // Overwrite any existing value in the entry. The implementation should
    // use Inc() and check whether to push against push_policy_. The
    // implementation also needs to be thread-safe.  Return 0 on success, -1
    // otherwise.
    inline int DoPut(int32_t row_id, int32_t col_id, V new_val);

    // Increment an entry by delta. The implementation should check whether to
    // push against push_policy_. The implementation also needs to be
    // thread-safe.  Return 0 on success, -1 otherwise.
    //
    // Comment(wdai): User can generalize Inc() by overloading operator+ and
    // operator- of V to any (abelian) operation.
    inline int DoInc(int32_t row_id, int32_t col_id, V delta);

    // Iterate may invoke communication actions triggered by CheckIterate(),
    // but since we maintain clock in Driver and not at table level, Iterate()
    // here does not change any actual clock. Return 0 on success, negatives
    // on failure.
    inline int DoIterate();

    // RegisterThread blocks until it is called num_threads_ times (by
    // differnet threads).
    void RegisterThread();

  private:
    typedef ThreadSafeLRURowStorage<ROW, V> ProcessCache;

    // Simplify verbose derived type syntax.
    typedef typename EntryOp<V>::OpType OpType;

    // Apply a row 
    int AddRowToThreadCache(int32_t row_id, const ROW<V>& new_row);

    // Apply oplog (row_id, col_id), if anyy, to val and return the
    // read-my-write view.
    inline V ApplyOpLog(int32_t row_id, int32_t col_id, V val);


    /* TODO(wdai).
    // DecisionInfo has access to tablet_storage and OpLog in
    // ConsistencyController::thread_table_, which could be supplied to
    // ConsistencyPolicy decision-making at runtime.
    template<typename ROW_ID, typename V>
    class DecisionInfo {
      public:
        // Need to maintain a reference to the outer class to access its
        // private members.
        DecisionInfo(ConsistencyController& controller) :
          controller_(controller) { }

        // Get the net delta of an entry.
        V GetDelta(int32_t row_id, int32_t col_id) const;

      private:
        // Maintain a reference to the outer class.
        ConsistencyController<ROW_ID, V>& controller_;
    };
    */

  private:   // private members variables
    // DecisionInfo is supplied for ConsistencyPolicy to make decision.
    //DecisionInfo decision_info_;

    ConsistencyControllerConfig<V> controller_config_;

    int32_t table_id_;

    int32_t num_servers_;

    // The table (a collection of rows) are stored in tablet_storage_.
    boost::scoped_ptr<ProcessCache> process_cache_;

    // Thread specific info on top of the processor tablet_storage_.
    boost::thread_specific_ptr<ThreadTableInfo<ROW, V> > thread_table_info_;

    // ConsistencyController synchronizes with server thorugh ClientProxy.
    // ConsistencyController does not take ownership of proxy_.
    ClientProxy<V>* proxy_;

    // Consistency policies.
    ConsistencyPolicy<V>* policy_;

    // barrier_ is used to synchronize RegisterThread.
    boost::scoped_ptr<boost::barrier> barrier_;

    // Thread uses nth_thread_ to find its ID, which is used in vec_clock_.
    std::atomic<int32_t> nth_thread_;

    // Process clock.
    VectorClock vector_clock_;

  boost::thread_specific_ptr<StatsObj> &thr_stats_;
  std::string table_id_str; // just to facilitate StatsObj
};

// ================ ConsistencyController Implementation ================

template<template<typename> class ROW, typename V>
ConsistencyController<ROW, V>::ConsistencyController(
				     boost::thread_specific_ptr<StatsObj> &thr_stats):
  thr_stats_(thr_stats){ }

template<template<typename> class ROW, typename V>
ConsistencyController<ROW, V>::ConsistencyController(
			const ConsistencyControllerConfig<V>& controller_config,
			boost::thread_specific_ptr<StatsObj> &thr_stats):
 thr_stats_(thr_stats){
  Init(controller_config);
}

template<template<typename> class ROW, typename V>
void ConsistencyController<ROW, V>::Init(
    const ConsistencyControllerConfig<V>& controller_config) {
  controller_config_ = controller_config;

  // policy_
  CHECK_NOTNULL(controller_config.policy);
  policy_ = controller_config.policy;

  // proxy_
  CHECK_NOTNULL(controller_config.proxy);
  proxy_ = controller_config.proxy;

  table_id_ = controller_config.table_id;
  num_servers_ = controller_config.num_servers;

  // process_cache_
  StorageConfig storage_config = controller_config.storage_config;
  VLOG(3) << "Creating process cache of capacity "
    << storage_config.capacity;
  if (storage_config.lru_params_are_defined) {
    process_cache_.reset(new ProcessCache(
          storage_config.capacity,
          storage_config.active_list_size,
          storage_config.num_row_access_to_active));
  } else {
    process_cache_.reset(new ProcessCache(storage_config.capacity));
  }

  CHECK_GT(controller_config.num_threads, 0)
    << "A client needs to have >0 number of threads.";
  barrier_.reset(new boost::barrier(controller_config.num_threads));
  nth_thread_ = 0;

  std::stringstream ss;
  ss << table_id_;
  table_id_str = std::string("Table.") + ss.str();

  VLOG(3) << "ConsistencyController created for table " << table_id_;

}

template<template<typename> class ROW, typename V>
void ConsistencyController<ROW, V>::RegisterThread() {

  if (!thread_table_info_.get()) {
    thread_table_info_.reset(new ThreadTableInfo<ROW, V>(
          nth_thread_++,
          controller_config_.op_log_config,
          *thr_stats_, table_id_));
    // Register with vector_clock_;
    CHECK_EQ(0, vector_clock_.AddClock(thread_table_info_->get_thread_id()));
    VLOG(3) << "Initialized thread_table_info_ for thread_id = "
      << thread_table_info_->get_thread_id();
    // Wait for all threads have registered with vector_clock_.
    barrier_->wait();
  } else {
    LOG(FATAL) << "Registering a thread twice is disallowed.";
  }
  VLOG(3) << "Moving past barier since all " << controller_config_.num_threads
    << " threads are registered.";
#ifdef PETUUM_stats
  StatsObj::FieldConfig fconfig;                                                                     
  thr_stats_->RegField("InsertThreadCacheRow.microsec", fconfig);                     
  thr_stats_->RegField("ProcessCacheGetPutRow.microsec", fconfig);
  thr_stats_->RegField("ThreadCacheGetRowUnsafe", fconfig);
  thr_stats_->RegField("FetchServerRow", fconfig);
#endif 
}

template<template<typename> class ROW, typename V>
int ConsistencyController<ROW, V>::DoGet(int32_t row_id, int32_t col_id, V* val) {
  // We require the row_iter to be >= stalest_row_iter.
  int32_t curr_iter = thread_table_info_->GetThreadIter();
  int32_t stalest_row_iter = policy_->GetStalestRowIter(row_id, col_id,
      curr_iter);

  VLOG(3) << "(row_id, col_id) = (" << row_id << ", " << col_id
    << "); curr_iter = " << curr_iter << " stalest_row_iter = "
    << stalest_row_iter;

  // Attempt to read from thread cache.
  OpLogManager<ROW, V>& op_log_manager = thread_table_info_->GetOpLogManager();
  V cache_val;
  int32_t row_iter;

  int thr_cache_hit = op_log_manager.Get(row_id, col_id, &cache_val, &row_iter);

  if(thr_cache_hit > 0) {
    // row_id is in thread cache. Check against policy now.
    if (row_iter >= stalest_row_iter) {
      VLOG(3) << "Got row " << row_id << " from thread cache. Iteration "
        << row_iter << " is fresher than the required = " << stalest_row_iter;
      // thread_cache_ is fresh enough!
      *val = cache_val;
      return 0;
    }
  }

  // Check process cache (get the entire row).
  VLOG(3) << "Requesting row " << row_id << " from process cache...";
  ROW<V> row;

  int proc_cache_hit = process_cache_->GetRow(row_id, &row, stalest_row_iter);

  if(proc_cache_hit > 0) {
    VLOG(3) << "Got row " << row_id << " from process cache.";
    // row_id is in process cache and is fresh enough! Put it into thread cache.
    op_log_manager.InsertThreadCache(row_id, row);

    // Get OpLog to ensure read-my-write.
    *val = ApplyOpLog(row_id, col_id, row[col_id]);

    return 0;
  }
  VLOG(1) << "Did not find row " << row_id
    << " in process cache. Requesting it from server...";

  // Block and fetch row from server.
  ROW<V> server_row;

  CHECK_EQ(0, proxy_->RequestGetRowRPC(table_id_, row_id, &server_row,
        stalest_row_iter));

  VLOG(3) << "Got row " << row_id << " from server with iteration "
    << server_row.get_iteration() << " fresher than required "
    << stalest_row_iter;
  CHECK_GE(server_row.get_iteration(), stalest_row_iter)
    << "Did not get fresh enough row from server. Report bug.";

  // Set return value.
  // Get OpLog to ensure read-my-write.
  *val = ApplyOpLog(row_id, col_id, server_row[col_id]);

  // Put server_row to both process_cache_ and thread_cache_.
  process_cache_->PutRow(row_id, server_row);

  op_log_manager.InsertThreadCache(row_id, server_row);
  return 0;
}

template<template<typename> class ROW, typename V>
ROW<V>& ConsistencyController<ROW, V>::DoGetRowUnsafe(int32_t row_id) {
  // We require the row_iter to be >= stalest_row_iter.
  int32_t curr_iter = thread_table_info_->GetThreadIter();
  // Always use column id = 0.
  int32_t stalest_row_iter = policy_->GetStalestRowIter(row_id, 0, curr_iter);

  VLOG(3) << "DoGetRowUnsafe row_id = " << row_id
    << "; curr_iter = " << curr_iter << " stalest_row_iter = "
    << stalest_row_iter;

  // Attempt to read from thread cache.
  OpLogManager<ROW, V>& op_log_manager = thread_table_info_->GetOpLogManager();
  int32_t row_iter;

#ifdef PETUUM_stats
  thr_stats_->StartTimer("ThreadCacheGetRowUnsafe");
#endif

  ROW<V>* row_ptr = op_log_manager.GetRowUnsafe(row_id, &row_iter);

#ifdef PETUUM_stats
  CHECK_GE(thr_stats_->EndTimer("ThreadCacheGetRowUnsafe"), 0);
#endif

  if(row_ptr != NULL) {
    // row_id is in thread cache. Check against policy now.
    if (row_iter >= stalest_row_iter) {
      VLOG(1) << "Got row " << row_id << " from thread cache. Iteration "
        << row_iter << " is fresher than the required = " << stalest_row_iter;
      // thread_cache_ is fresh enough!
      return *row_ptr;
    }
  }

  // Check process cache (get the entire row).
  VLOG(3) << "Requesting row " << row_id << " from process cache...";
  ROW<V> row;
  
#ifdef PETUUM_stats
  thr_stats_->StartTimer("ProcessCacheGetPutRow.microsec");
#endif
  int proc_cache_hit = process_cache_->GetRow(row_id, &row, stalest_row_iter);

#ifdef PETUUM_stats
  CHECK_GE(thr_stats_->EndTimer("ProcessCacheGetPutRow.microsec"), 0);
#endif

  if(proc_cache_hit > 0) {
    VLOG(3) << "Got row " << row_id << " from process cache.";
    // row_id is in process cache and is fresh enough! Put it into thread cache.


#ifdef PETUUM_stats
    thr_stats_->StartTimer("InsertThreadCacheRow.microsec");
#endif
    op_log_manager.InsertThreadCache(row_id, row);
    // Get it from thread cache.
#ifdef PETUUM_stats
    CHECK_GE(thr_stats_->EndTimer("InsertThreadCacheRow.microsec"), 0);
#endif
    
#ifdef PETUUM_stats
    thr_stats_->StartTimer("ThreadCacheGetRowUnsafe");
#endif
    ROW<V>* row_ptr2 = op_log_manager.GetRowUnsafe(row_id, &row_iter);
#ifdef PETUUM_stats
    CHECK_GE(thr_stats_->EndTimer("ThreadCacheGetRowUnsafe"), 0);
#endif

    if (row_ptr2 == NULL) {
      LOG(FATAL) << "We just inserted row " << row_id
        << " to thread cache. Cannot be null. Report bug.";
    }
    return *row_ptr2;
  }
  VLOG(3) << "Did not find row " << row_id
    << " in process cache. Requesting it from server...";

  // Block and fetch row from server.
  ROW<V> server_row;
  
#ifdef PETUUM_stats
  thr_stats_->StartTimer("FetchServerRow");
#endif

  CHECK_EQ(0, proxy_->RequestGetRowRPC(table_id_, row_id, &server_row,
        stalest_row_iter));

#ifdef PETUUM_stats
  CHECK_GE(thr_stats_->EndTimer("FetchServerRow"), 0);
#endif
  
  VLOG(3) << "Got row " << row_id << " from server with iteration "
    << server_row.get_iteration() << " fresher than required "
    << stalest_row_iter;
  CHECK_GE(server_row.get_iteration(), stalest_row_iter)
    << "Did not get fresh enough row from server. Report bug.";

  // Set return value.
  // Put server_row to both process_cache_ and thread_cache_.

#ifdef PETUUM_stats
  thr_stats_->StartTimer("ProcessCacheGetPutRow.microsec");
#endif

  process_cache_->PutRow(row_id, server_row);

#ifdef PETUUM_stats
  CHECK_GE(thr_stats_->EndTimer("ProcessCacheGetPutRow.microsec"), 0);
#endif

#ifdef PETUUM_stats
  thr_stats_->StartTimer("InsertThreadCacheRow.microsec");
#endif
  
  op_log_manager.InsertThreadCache(row_id, server_row);

#ifdef PETUUM_stats
  CHECK_GE(thr_stats_->EndTimer("InsertThreadCacheRow.microsec"), 0);
#endif

  // Get it from thread cache.
#ifdef PETUUM_stats
  thr_stats_->StartTimer("ThreadCacheGetRowUnsafe");
#endif
  ROW<V>* row_ptr3 = op_log_manager.GetRowUnsafe(row_id, &row_iter);
#ifdef PETUUM_stats
  CHECK_GE(thr_stats_->EndTimer("ThreadCacheGetRowUnsafe"), 0);
#endif  

  if (row_ptr3 == NULL) {
    LOG(FATAL) << "We just inserted row " << row_id
      << " to thread cache. Cannot be null. Report bug.";
  }
  return *row_ptr3;
}

template<template<typename> class ROW, typename V>
V ConsistencyController<ROW, V>::ApplyOpLog(int32_t row_id, int32_t col_id,
    V val) {
  OpLogManager<ROW, V>& op_log_manager = thread_table_info_->GetOpLogManager();
  EntryOp<V> oplog;
  if (op_log_manager.GetOpLog(row_id, col_id, &oplog) > 0) {
    switch (oplog.op) {
      case OpType::INC:
        return val + oplog.val;
      case OpType::PUT:
        return oplog.val;
      default:
        LOG(FATAL) << "Unrecognized operation in oplog: " << oplog.op;
    }
  }
  return val;
}

template<template<typename> class ROW, typename V>
int ConsistencyController<ROW, V>::DoPut(int32_t row_id, int32_t col_id,
    V new_val) {
  OpLogManager<ROW, V>& op_log_manager = thread_table_info_->GetOpLogManager();
  if (policy_->PutChecker(row_id, col_id, new_val) != NO_OP) {
    LOG(FATAL) << "Not implemented.";
  }
  CHECK_LE(0, op_log_manager.Put(row_id, col_id, new_val))
    << "Some Oplog error.";
  /*
  // TODO(wdai): Implement op log size control.
  if (op_log_manager.Put(row_id, col_id, new_val) > 0) {
  // OpLog hits capacity. Flush it to process cache and server.
  boost::shared_array<uint8_t> op_log_bytes =
  op_log_manager.SerializeOpLogTable();
  CHECK_EQ(0, proxy_.SendOpLogs(table_id_, op_log_bytes));
  op_log_manager.ClearOpLogTable();
  }
  */
  return 0;
}

template<template<typename> class ROW, typename V>
int ConsistencyController<ROW, V>::DoInc(int32_t row_id, int32_t col_id,
    V delta) {
  OpLogManager<ROW, V>& op_log_manager = thread_table_info_->GetOpLogManager();
  if (policy_->IncChecker(row_id, col_id, delta) != NO_OP) {
    LOG(FATAL) << "Not implemented.";
  }
  CHECK_LE(0, op_log_manager.Inc(row_id, col_id, delta));
  return 0;
}

template<template<typename> class ROW, typename V>
int ConsistencyController<ROW, V>::DoIterate() {
  // TODO(wdai): Use IterateChecker.
  OpLogManager<ROW, V>& op_log_manager = thread_table_info_->GetOpLogManager();

  // Serialize op log by server.
  std::vector<boost::shared_array<uint8_t> > op_log_bytes_by_server;
  std::vector<int> num_bytes_by_server;
  boost::shared_array<uint8_t> op_log_bytes;
  op_log_manager.SerializeOpLogTableByServer(&op_log_bytes_by_server,
            &num_bytes_by_server);
  op_log_manager.ClearOpLogTable();
  thread_table_info_->IncrementThreadIter();

  if (vector_clock_.Tick(thread_table_info_->get_thread_id()) > 0) {
    // I'm the slowest thread on this process. Iterate
    int client_id = thread_table_info_->get_thread_id();
    VLOG(2) << "Thread " << client_id
      << " iterates to iteration "
      << vector_clock_.get_client_clock(client_id);
    int ret = 0;
    for (int server_id = 0; server_id < num_servers_; ++server_id) {
      // Send iterate too all servers
      VLOG(2) << "SendIterate to server_id = " << server_id << " of bytes "
          << num_bytes_by_server[server_id];;
      ret |= proxy_->SendIterate(table_id_, server_id,
				 op_log_bytes_by_server[server_id], 
				 num_bytes_by_server[server_id]);
    }
    return ret;
  } else {
    // Send OpLog.
    int client_id = thread_table_info_->get_thread_id();
    VLOG(2) << "Thread " << client_id
      << " send oplog without iterating (thread advances to iteration "
      << vector_clock_.get_client_clock(client_id) << ")";
    int ret = 0;
    for (int server_id = 0; server_id < num_servers_; ++server_id) {
      //if (num_bytes_by_server[server_id] > sizeof(int)) {
        // There is at least 1 oplog to server_id
        VLOG(2) << "SendOpLogs to server_id = " << server_id << " of bytes "
          << num_bytes_by_server[server_id];
        ret |= proxy_->SendOpLogs(table_id_, server_id,
				  op_log_bytes_by_server[server_id], 
				  num_bytes_by_server[server_id]);
      //}
    }
    return ret;
  }
}

}  // namespace petuum

#endif  // PETUUM_CONSISTENCY_CONTROLLER
