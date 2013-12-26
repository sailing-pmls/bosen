// Copyright (c) 2013, SailingLab
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the <ORGANIZATION> nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef PETUUM_SPARSE_ROW
#define PETUUM_SPARSE_ROW

#include "petuum_ps/storage/row_interface.hpp"
#include <boost/unordered_map.hpp>


namespace petuum {

template<typename V>
class SparseRow : public Row<V> {
  public:
    // this is added according to new interface
    struct SparseRowStatus {
      int32_t num_columns_;
      int32_t iteration_;
      SparseRowStatus(){}
      SparseRowStatus(int32_t _num_columns, int32_t _iterations):
        num_columns_(_num_columns), iteration_(_iterations){}
    };

    SparseRow();

    // num_columns must be set at creation.
    SparseRow(int32_t num_columns);

    SparseRow(const boost::unordered_map<int32_t,V>& map);

    // iteration represent the iteration timestamp of this row.
    SparseRow(int32_t num_columns, int32_t iteration);

    // iteration represent the iteration timestamp of this row.
    SparseRow(const boost::unordered_map<int32_t,V>& map, int32_t iteration);

    int32_t get_num_columns() const;

    const V& operator[](int32_t col_id) const;
    V& operator[](int32_t col_id);

    // Operations on iteration_
    void set_iteration(int32_t new_iteration);
    int32_t get_iteration() const;

    int32_t Serialize(boost::shared_array<uint8_t> &_bytes) const;
    int Deserialize(const boost::shared_array<uint8_t> &_bytes,
        int32_t _nbytes);

  private:
    mutable boost::unordered_map<int32_t,V> row_data_;
    SparseRowStatus row_status_;
};

// =================== SparseRow Implementation ===================


template<typename V>
SparseRow<V>::SparseRow() : row_status_(0, 0), row_data_() { }

template<typename V>
SparseRow<V>::SparseRow(int32_t num_columns) :
  row_status_(num_columns, 0), row_data_() { }

template<typename V>
SparseRow<V>::SparseRow(const boost::unordered_map<int32_t,V>& map) :
  row_status_(map.size(), 0), row_data_(map) { }

template<typename V>
SparseRow<V>::SparseRow(int32_t num_columns,
    int32_t iteration) :
  row_status_(num_columns, iteration),
  row_data_() { }

template<typename V>
SparseRow<V>::SparseRow(const boost::unordered_map<int32_t,V>& map,
    int32_t iteration) :
  row_status_(map.size(), iteration),
  row_data_(map){ }

template<typename V>
const V& SparseRow<V>::operator[](int32_t col_id) const {
  // check for out of bound access
  if (col_id >= row_status_.num_columns_) {
    LOG(FATAL) << "Request exceeding the row size. exiting -- SEGFAULT."
	       << " num_columns = " << row_status_.num_columns_
	       << " trying to access: " << col_id;
    exit(1);
  }
  // handle not found
  static const V zero_ = V();
  if (row_data_.find(col_id) == row_data_.end())
  {
    return zero_;
  }
  // if found, check whether it's 0.
  if (row_data_[col_id] == 0) {
    row_data_.erase(col_id);
    return zero_;
  }
  return row_data_[col_id];
}

template<typename V>
V& SparseRow<V>::operator[](int32_t col_id) {
  // if not outside the range, return the last element
  if (col_id >= row_status_.num_columns_) {
    LOG(FATAL) << "Request exceeding the row size. exiting -- SEGFAULT."
	       << " num_columns = " << row_status_.num_columns_
	       << " trying to access: " << col_id;
    exit(1);
  }
  return row_data_[col_id];
}

template<typename V>
int32_t SparseRow<V>::get_num_columns() const {
  return row_status_.num_columns_;
}

template<typename V>
void SparseRow<V>::set_iteration(int32_t new_iteration) {
  row_status_.iteration_ = new_iteration;
}

template<typename V>
int32_t SparseRow<V>::get_iteration() const {
  return row_status_.iteration_;
}


// data beyond num_columns is discarded
template<typename V>
int32_t SparseRow<V>::Serialize(boost::shared_array<uint8_t> &_bytes) const {
  VLOG(2) << "Serializing num_columns = " << row_status_.num_columns_
    << " with row iteration = " << row_status_.iteration_;
  // Put (col_id, V) pair.
  int32_t nbytes = sizeof(SparseRowStatus) +
    (row_data_.size()) * (sizeof(int32_t) + sizeof(V));
  uint8_t *dptr = new uint8_t[nbytes];
  memcpy(dptr, &row_status_, sizeof(SparseRowStatus));
  typename boost::unordered_map<int32_t, V>::const_iterator it
    = row_data_.begin();
  int ientry = 0;
  for (;it != row_data_.end(); ++it) {
    uint8_t* curr_ptr = dptr + sizeof(SparseRowStatus)
        + ientry * (sizeof(int32_t) + sizeof(V));
    int32_t* col_ptr = reinterpret_cast<int32_t*>(curr_ptr);
    *col_ptr = it->first;
    V* val_ptr = reinterpret_cast<V*>(curr_ptr + sizeof(int32_t));
    *val_ptr = it->second;
    ++ientry;
  }
  _bytes.reset(dptr);
  return nbytes;
}

template<typename V>
int SparseRow<V>::Deserialize(const boost::shared_array<uint8_t> &_bytes,
           int32_t _nbytes){
  if ((uint32_t) _nbytes < sizeof(SparseRowStatus)) return -1;

  row_status_ = *((SparseRowStatus *) _bytes.get());
  int32_t num_nonzero = (_nbytes - sizeof(SparseRowStatus))
    / (sizeof(int32_t) + sizeof(V));
  row_data_.clear();
  uint8_t* dptr = _bytes.get() + sizeof(SparseRowStatus);
  const V zero = V();
  for (int i = 0; i < num_nonzero; i++) {
    uint8_t* entry_ptr = dptr + i * (sizeof(int32_t) + sizeof(V));
    int32_t* col_id = reinterpret_cast<int32_t*>(entry_ptr);
    V* val = reinterpret_cast<V*>(entry_ptr + sizeof(int32_t));
    if (*val != zero) {
      row_data_[*col_id] = *val;
    }
  }
  VLOG(2) << "Deserialized num_columns = " << row_status_.num_columns_
    << " with row iteration = " << row_status_.iteration_;
  return 0;
}

}  // namespace petuum

#endif  // PETUUM_SPARSE_ROW
