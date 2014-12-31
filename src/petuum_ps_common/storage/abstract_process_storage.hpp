// Author: Jinliang, Yihua Fang

#pragma once

#include <petuum_ps_common/include/row_access.hpp>
#include <petuum_ps_common/client/client_row.hpp>

#include <boost/noncopyable.hpp>

namespace petuum {

class AbstractProcessStorage : boost::noncopyable {
public:
  // capacity is the upper bound of the number of rows this ProcessStorage
  // can store.
  AbstractProcessStorage() { }

  virtual ~AbstractProcessStorage() { }

  // Look up for row_id. If found, return a pointer pointing to that row,
  // otherwise return 0.
  // If the storage is a evictable type, then the row_accessor is set
  // to the corresponding row, which maintains the reference count.
  // Otherwise, row_accessor is not used and can actually be a NULL pointer.
  virtual ClientRow *Find(int32_t row_id, RowAccessor* row_accessor) = 0;

  virtual bool Find(int32_t row_id) = 0;

  // Insert a row, and take ownership of client_row.
  // Insertion failes if the row has already existed and return false; otherwise
  // return true.
  virtual bool Insert(int32_t row_id, ClientRow* client_row) = 0;
};


}  // namespace petuum
