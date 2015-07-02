// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.11.22

#include <string>
#include <cstdint>
#include <mutex>
#include <glog/logging.h>
#include <ml/disk_stream/byte_buffer.hpp>

namespace petuum {
namespace ml {

ByteBuffer::ByteBuffer() : curr_offset_(0) { }

void ByteBuffer::SetBuffer(std::vector<char>* bytes) {
  curr_offset_ = 0;
  bytes_.swap(*bytes);
}

void ByteBuffer::AdvanceOffset(int32_t num_bytes) {
  curr_offset_ += num_bytes;
  CHECK_LE(curr_offset_, bytes_.size()) << "num_bytes: " << num_bytes;
}

bool ByteBuffer::HasMore() const {
  return curr_offset_ != bytes_.size();
}

char* ByteBuffer::GetNextBytes() {
  return bytes_.data() + curr_offset_;
}

}  // namespace ml
}  // namespace petuum
