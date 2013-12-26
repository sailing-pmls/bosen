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

/*
 * table_row.hpp
 *
 *  Created on: Sep 26, 2013
 *      Author: jinliang
 */

#ifndef __PETUUM_TABLE_ROW_HPP__
#define __PETUUM_TABLE_ROW_HPP__
#include <vector>
#include <boost/unordered_map.hpp>
namespace petuum{

  // abstract class for row data
  template<typename ValueType>
  class RowData{
  private:
    int32_t &ref_count_;

    // a reference to underlying data structure.
  public:
    ValueType operator[](int32_t _cid) = 0;
  };

  template<typename ValueType>
  class DenseRowData : public RowData<ValueType>{
  private:
    int32_t &ref_count_;
    std::vector<ValueType> &row_;
    // a reference to underlying data structure.
  };

  template<typename ValueType>
  class SparseRowData : public RowData<ValueType>{
  private:
    int32_t &ref_count_;

    // a reference to underlying data structure.
  };

  template<typename ValueType>
  class RowBatch {
  public:
    int Put(int32_t _cid, ValueType _v);
    int Inc(int32_t _cid, ValueType _v);
  };
}


#endif
