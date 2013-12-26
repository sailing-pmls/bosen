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

#ifndef PETUUM_DENSE_ROW
#define PETUUM_DENSE_ROW

#include "petuum_ps/storage/row_interface.hpp"
#include <glog/logging.h>
#include <vector>

namespace petuum {

// Use std::vector as the dense row container, plus a iteration number
// associated with this row. Note that we always include iteration since
// it's only an int32_t.
template<typename V>
class DenseRow : public Row<V> {
  public:
  struct DenseRowStatus {
    int32_t num_columns_;
    int32_t iteration_;
    DenseRowStatus(){}
    DenseRowStatus(int32_t _num_columns, int32_t _iterations):
      num_columns_(_num_columns),
      iteration_(_iterations){}
  };

  DenseRow();

  // num_columns must be set at creation.
  DenseRow(int32_t num_columns);

  DenseRow(const std::vector<V>& vec);

  // iteration represent the iteration timestamp of this row.
  DenseRow(int32_t num_columns, int32_t iteration);

  // iteration represent the iteration timestamp of this row.
  DenseRow(const std::vector<V>& vec, int32_t iteration);

  // Comment(wdai): Use implicit copy constructor and assignment operator.

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
  DenseRowStatus row_status_;
  std::vector<V> row_data_;
};

// =================== DenseRow Implementation ===================

// Use default constructors on members.
template<typename V>
DenseRow<V>::DenseRow() : row_status_(0, 0) { }

template<typename V>
DenseRow<V>::DenseRow(int32_t num_columns) :
  row_status_(num_columns, 0), row_data_(num_columns) { }

template<typename V>
DenseRow<V>::DenseRow(const std::vector<V>& vec) :
  row_status_(vec.size(), 0), row_data_(vec) { }

template<typename V>
DenseRow<V>::DenseRow(int32_t num_columns,
		      int32_t iteration) :
  row_status_(num_columns, iteration),
  row_data_(num_columns) { }

template<typename V>
DenseRow<V>::DenseRow(const std::vector<V>& vec,
    int32_t iteration) :
  row_status_(vec.size(), iteration),
  row_data_(vec){ }

template<typename V>
const V& DenseRow<V>::operator[](int32_t col_id) const {
  if (col_id >= row_status_.num_columns_) {
    LOG(FATAL) << "Request exceeding the vector size. "
	       << " num_columns = " << row_status_.num_columns_
	       << " requested col = " << col_id;
    // never reach.
    return row_data_[row_status_.num_columns_ - 1];
  }
  return row_data_.at(col_id);
}

template<typename V>
V& DenseRow<V>::operator[](int32_t col_id) {
  // at throws exception if out of bound.
  if (col_id >= row_status_.num_columns_) {
    LOG(FATAL) << "Request exceeding the vector size. "
	       << " num_columns = " << row_status_.num_columns_
	       << " requested col = " << col_id;
    // never reach.
    return row_data_[row_status_.num_columns_ - 1];
  }
  return row_data_[col_id];
}

template<typename V>
int32_t DenseRow<V>::get_num_columns() const {
  return row_status_.num_columns_;
}

template<typename V>
  
void DenseRow<V>::set_iteration(int32_t new_iteration) {
  row_status_.iteration_ = new_iteration;
}

template<typename V>
int32_t DenseRow<V>::get_iteration() const {
  return row_status_.iteration_;
}

// data beyond num_columns is discarded
template<typename V>
int32_t DenseRow<V>::Serialize(boost::shared_array<uint8_t> &_bytes) const {
  VLOG(2) << "Serializing num_columns = " << row_status_.num_columns_
    << " with row iteration = " << row_status_.iteration_;
  int32_t nbytes = sizeof(DenseRowStatus) +
    (row_status_.num_columns_) * sizeof(V);
  uint8_t *dptr = new uint8_t[nbytes];
  memcpy(dptr, &row_status_, sizeof(DenseRowStatus));
  memcpy(dptr + sizeof(DenseRowStatus), row_data_.data(),
   nbytes - sizeof(DenseRowStatus));
  _bytes.reset(dptr);
  return nbytes;
}

template<typename V>
int DenseRow<V>::Deserialize(const boost::shared_array<uint8_t> &_bytes,
           int32_t _nbytes){
  if ((uint32_t) _nbytes < sizeof(DenseRowStatus)) return -1;

  row_status_ = *((DenseRowStatus *) _bytes.get());
  int32_t dsize = _nbytes - sizeof(DenseRowStatus);
  row_data_.resize(dsize);
  memcpy(row_data_.data(), _bytes.get() + sizeof(DenseRowStatus), dsize);
  VLOG(2) << "Deserialize num_columns = " << row_status_.num_columns_
    << " with row iteration = " << row_status_.iteration_;
  return 0;
}

}  // namespace petuum

#endif  // PETUUM_DENSE_ROW
