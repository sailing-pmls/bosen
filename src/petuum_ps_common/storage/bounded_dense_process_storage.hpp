// Author: Jinliang, Yihua Fang

#pragma once


#include <petuum_ps_common/storage/abstract_process_storage.hpp>
#include <functional>
#include <boost/noncopyable.hpp>

namespace petuum {

// BoundedDenseProcessStorage assumes the rows are in a contiguous range
// and the maximum number of rows existed cannot exceeed capacity.

class BoundedDenseProcessStorage : public AbstractProcessStorage {
public:
  typedef std::function<ClientRow*(int32_t)> CreateClientRowFunc;

  BoundedDenseProcessStorage(size_t capacity,
                             CreateClientRowFunc CreateClientRow,
                             int32_t offset);

  ~BoundedDenseProcessStorage();

  ClientRow *Find(int32_t row_id, RowAccessor* row_accessor);

  bool Find(int32_t row_id);

  // Insert a row, and take ownership of client_row.
  // Insertion failes if the row has already existed and return false; otherwise
  // return true.
  bool Insert(int32_t row_id, ClientRow* client_row);

private:
  inline ClientRow *FindOffset(int32_t row_id);
  inline ClientRow *FindNoOffset(int32_t row_id);

  typedef ClientRow* (BoundedDenseProcessStorage::*FindFunc)(int32_t row_id);

  std::vector<ClientRow*> storage_vec_;

  const size_t capacity_;
  CreateClientRowFunc CreateClientRow_;
  FindFunc Find_;
  const int32_t offset_;
};

}  // namespace petuum
