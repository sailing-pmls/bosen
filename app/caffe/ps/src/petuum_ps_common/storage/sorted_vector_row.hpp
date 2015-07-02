#pragma once

#include <petuum_ps_common/storage/numeric_store_row.hpp>
#include <petuum_ps_common/storage/sorted_vector_store.hpp>

namespace petuum {

template<typename V>
using SortedVectorRowCore  = NumericStoreRow<SortedVectorStore, V >;

template<typename V>
class SortedVectorRow : public SortedVectorRowCore<V> {
public:
  SortedVectorRow() { }
  ~SortedVectorRow() { }

  V operator [](int32_t col_id) const {
    std::unique_lock<std::mutex> lock(SortedVectorRowCore<V>::mtx_);
    return SortedVectorMapRowCore<V>::store_.Get(col_id);
  }

  // Bulk read. Thread-safe.
  void CopyToVector(std::vector<Entry<V> > *to) const {
    std::unique_lock<std::mutex> lock(SortedVectorRowCore<V>::mtx_);
    SortedVectorRowCore<V>::store_.Copy(to);
  }
};

}
