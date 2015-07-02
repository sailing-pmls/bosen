// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.11.22

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include <boost/utility.hpp>

namespace petuum {
namespace ml {

// ByteBuffer is a simple, non-thread-safe byte storage.
class ByteBuffer : boost::noncopyable{
public:
  ByteBuffer();

  // bytes will be swapped with bytes_. bytes will become arbitrary. This is
  // not thread safe so no other function should be called during SetBuffer.
  void SetBuffer(std::vector<char>* bytes);

  // Advance the read offset. Fail if advancing beyond the buffer end.
  void AdvanceOffset(int32_t num_bytes);

  // True if the buffer is not exhausted yet.
  bool HasMore() const;

  // Get pointer to the next byte.
  char* GetNextBytes();

private:
  // byte storage.
  std::vector<char> bytes_;

  // Current read offset.
  int32_t curr_offset_;

  std::mutex mtx_;
};

}  // namespace ml
}  // namespace petuum
