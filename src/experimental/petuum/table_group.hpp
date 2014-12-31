#ifndef PETUUM_DRIVER
#define PETUUM_DRIVER

namespace petuum {

// Forward declaration
template<typename ROW_ID, typename V>
class ThreadExecutionData;


// Driver is a singleton providing global operations that are outside of any
// table, e.g., Iterate(), thread-level execution data. It also provides a map
// from table ID to table  Some of the user interface resembles that of MPI as
// Driver offers cross-machine operations like GlobalBarrier().
//
// Comment (wdai): In LT LazyTables plays the role of Driver and TableGroups.
// Here we divorce them for clarity.
class TableGroup {
  public:
    // Singleton "constructor."
    static Driver& GetInstance() {
      static Driver instance;
      return instance;
    }

    // Increment the iteration #. This triggers Iterate() on all tables and
    // subsequently invoke CheckIterate().
    void Iterate();

    // GlobalBarrier ensures the view of the table is synchronized across all
    // machines and threads before returning.
    //
    // Comment(wdai): This is a feature requested by Jin.
    void GlobalBarrier();

  private:
    // Defeat the default constructor.
    Driver() { }

    // Defeat copy constructors and assignment. Do not implement.
    Driver(Driver const&);
    void operator=(Driver const&);

    // There will be (# of threads) clocks in the vector_clock_ to keep track
    // of the iteration of all threads.
    VectorClock vector_clock_;

    // Most of thread-level run time data, excluding data related to thread's
    // local view of table such as OpLog and ThreadCache, which are in
    // ThreadTableView.
    thread_specific_ptr<ThreadExecutionData> thread_exec_data_;

    // Table ID (int) mapped to ConsistencyTable (Table).
    std::map<int, Table> tables_;
};

// Most of the thread-level information should be centralized in
// ThreadExecutionData, including execution data like thread iteration number.
// This does not include data needed to construct thread-view of the table
// (they are in ThreadTableView).
//
// Comment(wdai): Performance statistics could go here.
template<typename ROW_ID, typename V>
class ThreadExecutionData {
  public:
    // Accessors and mutators
    int get_thread_id() const;
    void set_thread_id(int new_thread_id);

    int get_thread_iter() const;
    void set_thread_iter();

  private:
    // Thread ID within the processor. This is generally the rank of the
    // thread within the processor.
    int thread_id_;

    // The execution iteration, incremented by Iterate().
    int thread_iter_;
};

}  // namespace petuum

#endif  // PETUUM_DRIVER
