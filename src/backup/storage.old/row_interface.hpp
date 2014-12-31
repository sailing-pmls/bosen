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
