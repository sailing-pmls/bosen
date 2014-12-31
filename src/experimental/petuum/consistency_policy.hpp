#ifndef PETUUM_CONSISTENCY_POLICY
#define PETUUM_CONSISTENCY_POLICY

namespace petuum {

// ConsistencyPolicy defines communication triggers corresponding to each table operations:
// Get(), Inc(), Put(), and Iterate().
//
// Comment (wdai): Pre-fetching policies can be another member function of
// this class.
//
// TODO(wdai): PULL action will result in modifying processor-level table,
// which will block read from all threads. Need more thoughts on this.
template<typename ROW_ID, typename V>
class ConsistencyPolicy {
  public:
    // GetChecker is performed before every read of the table (Get functions).
    // Return PULL to trigger a blocking pull. In general the row_id is not
    // used, but user may optionally define different pull policies for
    // different rows. To do so the user could subclass this class and
    // maintain vector<ROW_ID> of rows of various policies. The default
    // implementation does nothing.
    //
    // Comment (wdai): It may be better to use iteration staleness = 1 (aka
    // BSP) as default.
    virtual CommAction GetChecker(const ROW_ID& row_id, int col_id,
        const Clock& execution_iter, const Clock& row_iter) {
      return NO_OP;
    }

    // PutChecker is performed before Put() changes the client view of a row
    // (in local cache). Currently The decision is made based on the row_id,
    // col_id, and new_value.
    virtual CommAction PutChecker(const ROW_ID& row_id, int col_id,
        const V& new_val) {
      return NO_OP;
    }

    // IncChecker is performed before Inc() changes the client view of a row
    // (in local cache).
    //
    // TODO(wdai): Refactor secondary information in PolicyDecisionInfo class to
    // generate those info upon request.
    virtual CommAction IncChecker(const ROW_ID& row_id, int col_id,
        const OpLog<V>& pending_update) {
      return NO_OP;
    }

    // IterateChecker is performed when calling Iterate() on the table. The
    // resulting CommAction is applied to all rows.
    //
    // TODO(wdai): Need a class (call it IterateChckerInfo) for user to access
    // the entire OpLogTable of the current thread. Let's not worry about it
    // for now until a good use case calls for it.
    virtual CommAction IterateChecker(int old_iter) {
      return NO_OP;
    }
};


// ReadWritePolicy limit the various consistency options in ConsistencyPolicy to
// two policy definitions: ReadChecker is performed before reading from the table
// and invoke PULL CommAction if necessary, and WriteChecker is performed
// before write (Put() and Inc()) to invoke PUSH CommAction.
//
// Comment(wdai): A more general write checker should allow PULL action as
// well to prevent pathelogical case where the application continues to wrote
// without reading a row.
template<typename ROW_ID, typename V>
class ReadWritePolicy : public ConsistencyPolicy<ROW_ID, V> {
  public:
    // ReadChecker is called before reading the table (Get()). It can only
    // return NO_OP or PULL.
    virtual CommAction ReadChecker(const ROW_ID& row_id, int col_id,
        const Clock& execution_iter, const Clock& row_iter) = 0;

    // WriteChecker is invoked for write operations (Put() and Inc()).
    // It can only return PUSH or NO_OP. pending_update should represent
    // the summary of total pending delta. For example, if the exeuction
    // issued two Inc() operations which are not yet pushed to the server,
    // then pending_update should represent the sum of the two Inc().
    //
    // Comment(wdai): It's OpLogTable or OpLogManager's job to produce such
    // summary delta before passing it to policy.
    //
    // Comment(wdai): It doesn't need to be a blocking push, but just need to
    // make sure to block subsequent read and write of this row by using
    // blocking flag. We leave it as future optimization.
    virtual CommAction WriteChecker(const ROW_ID& row_id, int col_id,
        const OpLog<V>& pending_update)  = 0;

    // === Implement policies based on ReadChecker and WriteChcker ===
    // Override with user-defined ReadChecker.
    CommAction GetChecker(const ROW_ID& row_id, int col_id,
        const Clock& execution_iter, const Clock& row_iter) {
      return ReadChecker(row_id, col_id, execution_iter, row_iter);
    }

    // Override with user-defined WriteChecker.
    CommAction IncChecker(const ROW_ID& row_id, int col_id,
        const DecisionInfo<ROW_ID, V>& info) {
      return WriteChecker(row_id, col_id, pending_update);
    }

    // Override with user-defined WriteChecker.
    CommAction IncChecker(const ROW_ID& row_id, int col_id,
        const DecisionInfo<ROW_ID, V>& info) {
      return WriteChecker(row_id, col_id, pending_update);
    }
};

}  // namespace petuum

#endif  // PETUUM_CONSISTENCY_POLICY
