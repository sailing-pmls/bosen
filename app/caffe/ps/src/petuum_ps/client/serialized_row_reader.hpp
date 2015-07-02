// author: jinliang

#include <boost/noncopyable.hpp>
#include <petuum_ps/thread/context.hpp>

namespace petuum {

// Provide sequential access to a byte string that's serialized rows.
// Used to facilicate server reading row data.

// st_separator : serialized_table_separator
// st_end : serialized_table_end

// Tables are serialized as the following memory layout
// 1. int32_t : table id, could be st_separator or st_end
// 2. int32_t : row id, could be st_separator or st_end
// 3. size_t : serialized row size
// 4. row data
// repeat 1, 2, 3, 4
// st_separator can not happen right after st_separator
// st_end can not happen right after st_separator

// Rules for serialization:
// The serialized row data is guaranteed to end when seeing a st_end or with
// finish reading the entire memory buffer.
// When seeing a st_separator, there could be another table or no table
// following. The latter happens only when the buffer reaches its memory
// boundary.

class SerializedRowReader : boost::noncopyable {
public:
  // does not take ownership
  SerializedRowReader(const void *mem, size_t mem_size):
      mem_(reinterpret_cast<const uint8_t*>(mem)),
      mem_size_(mem_size) {
  }
  ~SerializedRowReader() { }

  bool Restart() {
    offset_ = 0;
    current_table_id_ = *(reinterpret_cast<const int32_t*>(mem_ + offset_));
    offset_ += sizeof(int32_t);

    if (current_table_id_ == GlobalContext::get_serialized_table_end())
      return false;
    return true;
  }

  const void *Next(int32_t *table_id, int32_t *row_id, size_t *row_size) {
    // When starting, there are 4 possiblilities:
    // 1. finished reading the mem buffer
    // 2. encounter the end of an table but there are other tables following
    // (st_separator)
    // 3. encounter the end of an table but there is no other table following
    // (st_end)
    // 4. normal row data

    if (offset_ + sizeof (int32_t) > mem_size_)
      return NULL;
    *row_id = *(reinterpret_cast<const int32_t*>(mem_ + offset_));
    offset_ += sizeof(int32_t);

    do {
      if (*row_id == GlobalContext::get_serialized_table_separator()) {
        if (offset_ + sizeof (int32_t) > mem_size_)
          return NULL;

        current_table_id_ = *(reinterpret_cast<const int32_t*>(mem_ + offset_));
        offset_ += sizeof(int32_t);

        if (offset_ + sizeof (int32_t) > mem_size_)
          return NULL;

        *row_id = *(reinterpret_cast<const int32_t*>(mem_ + offset_));
        offset_ += sizeof(int32_t);
        // row_id could be
        // 1) st_separator: if the table is empty and there there are other
        // tables following;
        // 2) st_end: if the table is empty and there are no more table
        // following
        continue;
      } else if (*row_id == GlobalContext::get_serialized_table_end()) {
        return NULL;
      } else {
        *table_id = current_table_id_;
        *row_size = *(reinterpret_cast<const size_t*>(mem_ + offset_));
        offset_ += sizeof(size_t);
        const void *data_mem = mem_ + offset_;
        offset_ += *row_size;
        return data_mem;
      }
    }while(1);
  }

private:
  const uint8_t *mem_;
  size_t mem_size_;
  size_t offset_; // bytes to be read next
  int32_t current_table_id_;
};

}  // namespace petuum
