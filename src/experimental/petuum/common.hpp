#ifndef PETUUM_COMMON
#define PETUUM_COMMON

// This header file provides declarations used by most of Petuum classes.

namespace petuum {

// Clock represents iteration number of the execution.
typedef uint64_t Clock;

// CommAction represents communication actions options for the clients.
// ConsistencyPolicy will trigger communication actions from this enum list.
//
// TODO(wdai): SYNC (PUSH + PULL) is another useful action to prevent
// pathological case where an application only write one row but not reading
// from that row. But again, such case should be extremely rare.
enum CommAction {
  NO_OP,  // Client does not invoke communication with the server.
  PUSH,  // Client push updates to server
  PULL   // Client pull updates from server
};

}  // namespace petuum

#endif  // PETUUM_COMMON
