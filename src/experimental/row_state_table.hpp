#ifndef ROW_STATE_TABLE_H
#define ROW_STATE_TABLE_H

// General Note:
// Use a combination of template and class registration system to strike a
// balance between efficiency and user-friendliness. Use template for the
// value type of table due to efficiency reasons, and use class registration
// system to dynamically generate user-defined subclasses, e.g., subclasses of
// Accumulator and RowState, to avoid adding over three template types to
// every class we create, which could be ugly. The bottom line is, a lot of
// classes will be templated by just one typename V, and user will need to add
// a line of registration macro to user-defined classes.

// ============ RowState ============

// RowState is the abstract class for state associated with a row. 
//
// TODO(wdai): Provide simple RowState like one tracking a single double.
class RowState {
  public:
    // This method is called when the row is synced (by push or pull) with
    // other clients.
    virtual void reset() {}

    // Test if 'this' is greater or equal to 'other'. It is useful to compare
    // time and carry out opportunistic push to server in value bound mode.
    //
    // Comment (wdai): It's better to use operator overload, but calling
    // operators on base class will not trigger the right behaviors defined in
    // subclass. Thus we need the comparison functions to be member functions.
    virtual bool GE(const RowState& other) {
      return false;
    }
};

// This is equivalent to unit type.
// TODO(wdai): use singleton.
class EmptyRowState : public RowState {};
const EmptyRowState kempty_row_state;

// RowClock is used for iteration bound without value bound.
// Comment(wdai): This is like SharedRowCache.data_age in LT.
class RowClock : public RowState {
  public:
    RowClock() : clock_(0) { }

    // We allow implicit conversion from uint64_t to RowClock.
    RowClock(uint64_t clock) : clock_(clock) { }

    void reset() {
      clock_ = 0;
    }

    // Comment (wdai): Even though the code is clunky, it provides a unifying
    // interface for the table so the table doesn't need to have separate
    // methods for reading data with certain freshness.
    /*bool GE(const RowState& other) {
      if (const RowClock& other_clock =
          dynamic_cast<const RowClock&>(&other)) {
        return this.clock_ >= other_clock.clock_;
      }
      return false;
    }*/

    // It suffices to implement w.r.t. RowClock instead of RowState.
    bool GE(const RowClock& other) {
      return this.clock_ >= other_clock.clock_;
    }

    // TODO(wdai): Need accessor and mutators.
  private:
    uint64_t clock_;
}

// A user defined state endowed with RowClock. User that want both value and
// iteration bound can subclass RowStateWithClock.
//
// Comment (wdai): This might not be that necessary.
class RowStateWithClock : public RowState {
  // TODO(wdai)
};

// ============ Accumulators ============

// Accumulator is the centerpiece for value-bound staleness. User can
// flexibly implement entry-wise policy in Accumulate() or row-wise policy in
// RowTrigger(). Note that Accumulator applies same rule to all rows in a
// Table. If we want non-uniform behavior (such as row-pair synchronization),
// we'll need to implement subclasses that are endowed with bookkeeping info.
//
// Comment (wdai): To have dynamic value bound (e.g., bound changes as program
// executes), Accumulator needs to have access to ExecutionState.
template<typename V>
class Accumulator {
  public:
    // Accumulate implements entry-wise updates, and should in general be an
    // abelian operation. Implementations can update row_state when applying
    // update to a cell. When returning true, the update will still be applied
    // to the rest of the cells in the row, and push is triggered at the end,
    // with row state being reset(). Note the user can achieve dynamic value
    // bound through exec_state, which is also user defined.
    virtual bool Accumulate(const ExecutionState& exec_state, V* a,
        const V& b, RowState* row_state) = 0;

    // RowTrigger is applied at the end of a row update.
    virtual bool RowTrigger(const ExecutionState& exec_state,
        const RowState& row_state) {
      return false;
    }
};

// Simple accumulator does not need any state for each row. Users only need to
// implement Accumulate(a, b) function.
//
// TODO(wdai): Provide a collection of default Accumulators: Sum, Min, Max,
// Replace (This subsumes table operation Put if the user does not need any
// other form of accumulation).
template<typename V>
class SimpleAccumulator : public Accumulator<V> {
  public:
    virtual void Accumulate(V* a, const V& b) = 0;

    bool Accumulate(const ExecutionState& exec_state,
        V* a, const V& b, EmptyRowState* row_state) {
      Accumulate(a, b);
      return false;   // Never trigger push.
    }
};

// EntryTriggerAccumulator allows update to trigger based on entry update
// without row state. Implementations need to supply Accumulate and Fire
// functions. This is useful to implement 0 --> non-zero, non-zero --> zero type
// of push pattern.
template<typename V>
class EntryTriggerAccumulator : public SimpleAccumulator<V> {
  public:
    virtual void Accumulate(V* a, const V& b) = 0;

    // a is current value in the table; b is the value to be Accumulated.
    virtual bool Fire(const V& a, const V& b) = 0;

    bool Accumulate(const ExecutionState& exec_state,
        V* a, const V& b, EmptyRowState* row_state) {
      bool to_fire = Fire(*a, b);
      Accumulate(a, b, kempty_row_state);
      return to_fire;
    }
};


// ============ ExecutionState ============
//
// Local state associated with the execution thread to be accessed by user.
// This class does not need thread-safety as it is accessed by exactly 1
// thread. Note that internal execution state is represented by SystemState to
// block user access to internal statistics. We assume the user will always
// need iteration number.
struct ExecutionState {
  uint64_t iteration;
};

// Variables for system optimization, such as num_thr_cache_hit, etc. goes
// here. Also a thread level data.
struct SystemState {
};

// TODO(wdai): Could create a super state that includes both ExecutionState
// and SystemState to be used in thread_specific_ptr.

// ============ Pull Mechanism =============
class PullTrigger {
  public:
    // The Controller checks this condition at reading (Get methods). Default
    // implementation does not pull at all.
    virtual bool NeedToPullRow(const RowState& row_state,
        const ExecutionState& exec_state) {
      return false;
    }

    // Comment (wdai): There could be an opportunistic pull check
    // (prefetching) when writing to the row. We will need to define another
    // method for user to override.
};

// ============ Table ============

class RowID;
class ColumnID;

// Data structure to store a row.
class RowData;

// Table class is the base class for server table, processor-level cache and
// thread-level cache, providing the common functionalities of all three:
// Put(), Has(), Get(), Inc(), where Inc() makes use of an Accumulator
// instance stored as class member. The base class implementation is not
// thread-safe, as server table and thread-level caches do not need
// thread-safety. Processor cache will need to implement OpLog to provide
// thread-safety.
//
// Comment (wdai): It's kind of user-unfriendly to have so many template
// types. Consider using class registration system or remove some of them
// (like COL_ID type). Note that these templates will propogate to Controller
// and other classes that make use of Table.
template<typename ROW_ID, typename COL_ID, typename ROW_DATA, typename V>
class Table {
  public:
    // The constructor limit the heap size (in bytes) a Table can grow. This
    // can be controlled by the underlying storage.
    Table(const size_t& size, const Accumulator& accum);

    // Put() method family: The base class implementations are not thread-safe.
    // Entry-wise put. This should return error code if the rid is not currently
    // stored; otherwise overwrite single entry.
    virtual int PutEntry(const ROW_ID& rid, const COL_ID& cid, const V& value);

    // Row-wise put.
    virtual int PutRow(const ROW_ID& rid, const ROW_DATA& value);

    // Add a row entry with an initial row_state, overwriting existing
    // row_state for row rid.
    virtual int PutEntryWithState(const ROW_ID& rid, const COL_ID& cid, const V& value,
        const RowState& row_state);

    // Add a row with an initial row_state, overwriting existing
    // row_state for row rid.
    virtual int PutRowWithState(const ROW_ID& rid, const COL_ID& cid,
        const ROW_DATA& value, const RowState& row_state);

    // Has() method family:
    // This checks if a particular entry is initialized.
    //
    // Comment (wdai): This is probably not necessary because if a row exists
    // (checked by HasRow) then an entry must exist, since we disallow
    // creating a single entry without initializing the entire row.
    bool HasEntry(const ROW_ID& rid, const COL_ID& cid) const;

    // Return true if the row is stored.
    bool HasRow(const ROW_ID& rid) const;

    // Check if an entry has RowState greater or equal to min_state. This is
    // used primarily for iteration bound. Note that this relies on the
    // GE() in RowState. The default implementation of GE in RowState base
    // class will cause all this function call to return false.
    // Comment (wdai): Again, this is subsumed by HasRowWithStateGE(). See
    // HasEntry() comments.
    bool HasEntryWithStateGE(const ROW_ID& rid, const COL_ID& cid,
        const ROW_STATE& min_state) const;

    // The row access analog to HasWithStateGE
    bool HasRowWithStateGE(const ROW_ID& rid, const ROW_STATE& min_state) const;

    // Get() method family: They are const methods and can block if no local
    // rows/entries exists or satisfies the requirements (freshness, etc).

    // Inc() method family: The base class implementations simply calls accum_
    // and are not thread-safe.
    // Return true to trigger a push.
    virtual bool IncEntry(const ROW_ID& rid, const COL_ID& cid,
        const V& delta);

    virtual bool IncRow(const ROW_ID& rid, const ROW_DATA& delta);

  protected:
    // This is the heavy lifting part of the table storage...
    map<ROW_ID, ROW_DATA> rows_;
    map<ROW_ID, ROW_STATE> row_states_;
    Accumulator accum_;
};


// ServerTable might not need anything beyound Table...
class ServerTable : public Table<...> {

};

// ProcessorCache should supply methods to apply the OpLog, and Get() methods
// have to incorporate outstanding OpLog as well.
//
// Comment (wdai): I think it'd be cleaner to let ProcessorCache manage the
// OpLog (by exposing a simple interface to Controller without the detail of
// how to read and apply an outstanding OpLog).
class ProcessorCache : public Table<...> {
  private:
    OpLog op_log_;
};

// ThreadCache can use shared_ptr pointing to ProcessorCache to avoid
// duplicating the RowData, same as LazyTable's implementation.
//
// Comment (wdai): If we are using shared_ptr and not storing the ROW_DATA
// ourself, ThreadCache can't really reuse any of the Table functions, and it
// might not make sense to subclass in this case.:(
class ThreadCache : public Table<...> {
};

// TableGroup provide access to individual table of TABLE_T type using a table
// id. TABLE_T = ServerTable, ProcessorCache or ThreadCache.
//
// Comment (wdai): Could template on TABLE_ID as well if necessary.
template<typename TABLE_T>
class TableGroup {
  private:
    map<int, TABLE_T> tables_;
};

// ============ Controller ============

// Controller is a singleton and manages high level synchronization. It
// essentially forwards calls to a Table instance and push or pull depending on
// the returned value and apply the update from server.
template<typename ROW_ID, typename COL_ID, typename ROW_DATA, typename V>
class Controller {
  public:


  private:
    scoped_ptr<CommHandler> comm_;

    map<table_id_t, Table> tables_;
    TableGroup tables_;

    thread_specific_ptr<ExecutionState> threads_;

    // In iteration bound the processor clock time is the slowest of all
    // thread clocks.
    VectorClock processor_clock_;
};

// ============ Driver ============

// User will need to implement Run(). Driver helps hiding the boilerplate
// code such as MPI and Google library initialization.
class Driver {
  public:
    // This method will be executed repeatedly until it returns true. You can
    // think of it as the mapper program in map-reduce framework where the
    // input list is rows in the table. Reduce phase is simulated by the
    // synchronization over server.
    //
    // TODO(wdai): A "mapper" sort of function may not be sufficiently
    // expressive. Should provide other functions with access to more
    // low-level components.
    virtual bool RepeatRun(ExecutionState* exec_state) = 0;

    // Initialize all the tables. This is called and global barriered before
    // RepeatRun()
    virtual void InitializeTables() = 0;

    // Requested feature from Jin.
    void GlobalBarrier();
};

#endif  // ROW_STATE_TABLE_H
