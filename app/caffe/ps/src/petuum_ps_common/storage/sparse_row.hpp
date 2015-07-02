#pragma once

#include <petuum_ps_common/storage/numeric_store_row.hpp>
#include <petuum_ps_common/storage/map_store.hpp>

namespace petuum {

template<typename V>
using SparseRowCore  = NumericStoreRow<MapStore, V >;

template<typename V>
class SparseRow : public SparseRowCore<V> {
public:
  SparseRow() { }
  ~SparseRow() { }

  V operator [](int32_t col_id) const {
    std::unique_lock<std::mutex> lock(DenseRowCore<V>::mtx_);
    return SparseRowCore<V>::store_.Get(col_id);
  }

  // Bulk read. Thread-safe.
  void CopyToVector(std::vector<std::pair<int32_t, V> > *to) const {
    std::unique_lock<std::mutex> lock(DenseRowCore<V>::mtx_);
    SparseRowCore<V>::store_.Copy(to);
  }
};

}
