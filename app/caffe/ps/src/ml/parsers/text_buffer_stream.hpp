#pragma once

#include <string>
#include <memory>

// Take a sequence of text buffers and present a stream of tokens
// delimited by "delim".

namespace petuum {
namespace ml {

class TextBufferStream {
public:
  TextBufferStream(const std::string &delim,
                   size_t max_token_size),
    delim_(delim),
    max_token_size_(max_token_size),
    tmp_buff_(new char[max_token_size]),
    cursor_(0) { }

  ~TextBufferStream() {}

  void AssignBuffer(void *buff, size_t buff_size, bool is_last_buff) {
    if (cursor_ == 0) {
      cursor_ = reinterpret_cast<char*>(buff);
      if (is_last_buff) {
        end_of_buff_ = cursor_ + buff_size - 1;
      } else {

      }
    }
  }

private:
  char *FindTheLastDelim(char *end_of_buff, size_t buff_size) {
    char *cursor = end_of_buff - (delim_.size() - 1);

  }

  const std::string &delim_;
  const size_t max_token_size_;
  std::unique_ptr<char> tmp_buff_;

  char *cursor_;
  char *end_of_buff_; // last byte that can be read
};

}
}
