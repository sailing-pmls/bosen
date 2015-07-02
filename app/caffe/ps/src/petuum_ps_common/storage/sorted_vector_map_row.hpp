#pragma once

#include <petuum_ps_common/storage/numeric_store_row.hpp>
#include <petuum_ps_common/storage/sorted_vector_map_store.hpp>

namespace petuum {

template<typename V>
using SortedVectorMapRowCore  = NumericStoreRow<SortedVectorMapStore, V >;

template<typename V>
class SortedVectorMapRow : public SortedVectorMapRowCore<V> {
public:
  SortedVectorMapRow() { }
  ~SortedVectorMapRow() { }

  V operator [](int32_t col_id) const {
    std::unique_lock<std::mutex> lock(SortedVectorMapRowCore<V>::mtx_);
    return SortedVectorMapRowCore<V>::store_.Get(col_id);
  }

  // Bulk read. Thread-safe.
  void CopyToVector(std::vector<Entry<V> > *to) const {
    std::unique_lock<std::mutex> lock(SortedVectorMapRowCore<V>::mtx_);
    SortedVectorMapRowCore<V>::store_.CopyToVector(to);
  }
};

}
