#ifndef PETUUM_CONSISTENCY_POLICY
#define PETUUM_CONSISTENCY_POLICY

namespace petuum {

// CommAction represents communication actions options for the clients.
// ConsistencyPolicy will trigger communication actions from this enum list.
//
// TODO(wdai): SYNC (PUSH + PULL) is another useful action to prevent
// pathological case where an application only write one row but not reading
// from that row. But again, such case should be extremely rare.
enum CommAction {
  NO_OP,  // Client does not invoke communication with the server.
  PUSH,  // Client non-blocking (async) push updates to server
  PULL_BLOCK,   // Client blocking (sync) pull updates from server
  PULL_NONBLOCK   // Client non-blocking pull updates from server.
};

// ConsistencyPolicy defines communication triggers corresponding to each table operations:
// Get(), Inc(), Put(), and Iterate().
//
// Comment (wdai): Pre-fetching policies can be another member function of
// this class.
//
// TODO(wdai): PULL action will result in modifying processor-level table,
// which will block read from all threads. Need more thoughts on this.
template<typename V>
class ConsistencyPolicy {
  public:

    // GetStalestRowIter is performed before every entry read of the table
    // (through Get()).  Return the stalest tolerable row iteration. If the
    // row_iter in cache is staler than that, trigger a blocking pull.
    // (Returning 0 for no op) In general the row_id is not used, but user may
    // optionally define different staleness requirements for different rows.
    // To do so the user could subclass this class and maintain vector<int32_t>
    // of row_id to apply various policies.
    virtual int32_t GetStalestRowIter(int32_t row_id, int32_t col_id,
        int32_t curr_iter) = 0;

    // PutChecker is performed before Put() changes the client view of a row
    // (in local cache). PUSH can be returned in value-bound scheme. The policy
    // is based based on the row_id, col_id, and new_value.
    virtual CommAction PutChecker(int32_t row_id, int32_t col_id,
        V new_val) = 0;

    // IncChecker is performed before Inc() changes the client view of a row
    // (in local cache). PUSH can be returned to enable value-bound scheme.
    virtual CommAction IncChecker(int32_t row_id, int32_t col_id, V delta) = 0;

    // IterateChecker is performed when calling Iterate() on the table. The
    // resulting CommAction is applied to all rows.
    //
    // TODO(wdai): Need a class (call it IterateChckerInfo) for user to access
    // the entire OpLogTable of the current thread. Let's not worry about it
    // for now until a good use case calls for it.
    virtual CommAction IterateChecker(int old_iter) = 0;
};

// SSP Pulls at read (Get()) and pulls at Iterate().
//
// Comment(wdai): Since SSP has short functions, use inline.
template<typename V>
class SspPolicy : public ConsistencyPolicy<V> {
  public:
    // We accept rows with iteration (current_iter - num_iter_stale).
    // num_iter_stale is similar (but not equivalent) to BSP.
    explicit SspPolicy(int32_t num_iter_stale = 0) :
      num_iter_stale_(num_iter_stale) { }

    // Default is staleness = 1 (pseudo-BSP).
    virtual int32_t GetStalestRowIter(int32_t row_id, int32_t col_id,
        int32_t curr_iter) {
      return curr_iter - num_iter_stale_;
    }

    // The default is no push.
    inline virtual CommAction PutChecker(int32_t row_id, int32_t col_id,
        V new_val) {
      return NO_OP;
    }

    // The default no push.
    inline virtual CommAction IncChecker(int32_t row_id, int32_t col_id,
        V delta) {
      return NO_OP;
    }

    // SSP always pushes.
    inline virtual CommAction IterateChecker(int old_iter) {
      return PUSH;
    }

  private:
    int32_t num_iter_stale_;
};



}  // namespace petuum

#endif  // PETUUM_CONSISTENCY_POLICY
