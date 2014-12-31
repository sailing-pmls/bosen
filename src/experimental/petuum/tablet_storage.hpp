#ifndef PETUUM_TABLET_STORAGE
#define PETUUM_TABLET_STORAGE

namespace petuum {

// TODO(wdai): Provide a thread-level TabletStorage pointer that points to
// processor-level storage when local thread cache is not available.

// ThreadCache is a partial copy of the processor table.
//
// Comment(wdai): Perhaps this should subclass from a common tablet storage
// abstract class.
template<typename ROW_ID, typename V>
class ThreadCache;

// This is the processor-level storage. May share code with server tablet
// storage.
template<typename ROW_ID, typename V>
class TabletStorage;

template<typename ROW_ID, typename V>
class ThreadSafeTabletStorage : public TabletStorage;

}

#endif  // PETUUM_TABLET_STORAGE
