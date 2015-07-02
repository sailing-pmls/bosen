#pragma once

#include <boost/noncopyable.hpp>

namespace petuum {
namespace ml {

class LibsvmParser : boost::noncopyable {
public:
  LibsvmParser():
      buff_(0) { }
  ~LibsvmParser() { }

  // is_last - is this the last buffer to be processed?
  void AssignBuffer(void *buff, size_t buff_size) {

  }

  bool GetNext(int32_t *row_id, int32_t *col_id, float *value) { }

 private:
  void *buff_;
  size_t buff_size_;
  size_t offset_;
  size_t num_rows_;
  size_t num_cols_;
};

}
}
