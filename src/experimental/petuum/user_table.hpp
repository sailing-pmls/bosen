#ifndef PETUUM_USER_TABLE
#define PETUUM_USER_TABLE

namespace petuum {

// ConsistencyTable is a wrapper on ConsistencyController and provide
// convenience/higher level methods for user to interact with the underlying
// storage.
//
// Comment(wdai): Since the heavy-lifting is handled mostly by
// ConsistencyController, we delegate the thread-safety responsibilities to it
// as well. That is, there's no thread-safety measure in this class.
//
// Comment(wdai): Since we are delegating so many function calls to
// ConsistencyController, ConsistencyTable could be made into a subclass of
// ConsistencyController with the extension of high level user interface.
template<typename ROW_ID, typename V>
class ConsistencyTable {
  public:
    // Basic (low-level) table operations.
    //
    // TODO(wdai): Do we really need to return int? Can we just do bool? Same
    // for the functions in ConsistencyController and row batch methods below.
    int Get(const ROW_ID& row_id, int col_id);
    int Put(const ROW_ID& row_id, int col_id, const V& new_val);
    int Inc(const ROW_ID& row_id, int col_id, const V& delta);

    // Higher level user interface.
    int CreateRow(const ROW_ID& row_id);

    // Atomatic row operations, and useful for scenarios where entries
    // in a row needs to preserve some quantiy, like probability sums to 1.
    int GetRow(const ROW_ID& row_id);
    int IncRow(const ROW_ID& row_id, const RowData<V>& row_delta);
    int PutRow(const ROW_ID& row_id, const RowData<V>& row_new_val);

  private:
    // All table read and write will go through controller_ to ensure
    // consistency.
    ConsistencyController<ROW_ID, V> controller_;

    // Comment(wdai): Do not support Iterate() at Table level. Instead let
    // user use Iterate() in Driver.
    void Iterate();

    // Allow Driver::Iterate() to call Iterate().
    friend void Driver::Iterate();
};

// This is for user convenience, but obscure the underlying consistency
// semantic of the implementation.
typedef ConsistencyTable Table;

}  // namespace petuum

#endif  // PETUUM_USER_TABLE
