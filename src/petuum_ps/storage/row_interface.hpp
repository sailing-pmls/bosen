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

#ifndef PETUUM_ROW_INTERFACE
#define PETUUM_ROW_INTERFACE

#include <glog/logging.h>
#include <boost/shared_array.hpp>
#include <stdint.h>

namespace petuum {

// Row defines the common interfaces for all row types.
//
// TODO(wdai): An alternative is to use boost::variant type.
template<typename V>
class Row {
  public:
    // Accessor and mutator will check for out-of-bound error.
    virtual const V& operator[](int32_t col_id) const = 0;
    virtual V& operator[](int32_t col_id) = 0;

    // Get number of columns. This is needed for LRU count.
    virtual int32_t get_num_columns() const = 0;

    // Operations on iteration_
    virtual int32_t get_iteration() const = 0;
    virtual void set_iteration(int32_t new_iteration) = 0;

    // Return number of bytes.
    virtual int32_t Serialize(boost::shared_array<uint8_t> &_bytes) const = 0;
    virtual int Deserialize(const boost::shared_array<uint8_t> &_bytes,
        int32_t _nbytes) = 0;
};

}  // namespace petuum

#endif  // PETUUM_ROW_INTERFACE
