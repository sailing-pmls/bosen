// Copyright (c) 2014, Sailing Lab
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
// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.06.19

#ifndef LDA_LDA_DOC
#define LDA_LDA_DOC

#include <glog/logging.h>
#include <cstdlib>
#include <vector>
#include <cstdint>
#include <list>
#include <string>
#include <string.h>
#include <random>

namespace lda {

// The structure representing a document as a bag of words and the
// topic assigned to each occurrence of a word.  In term of Bayesian
// learning and LDA, the bag of words are ``observable'' data; the
// topic assignments are ``hidden'' data.

// A LDADoc object created via default constructor can be
// initialized in 2 ways:
// 1) Append a series of word-count pairs and call RandomInitTopics().
// All tokens have topic -1 before RandomInitTopics() is called.
// 2) Deserialize from exisiting LDADoc.
//
// Note: LDADoc is not thread safe.
class LDADoc {
public:
  LDADoc() : num_topics_(0) { }

  // Initialize the topic assignments to -1.
  void AppendWord(int32_t word_id, size_t count) {
    for (int i = 0; i < count; ++i) {
      tokens_.push_back(word_id);
      token_topics_.push_back(-1);
    }
  }

  inline int32_t GetToken(int32_t idx) const {
    return tokens_[idx];
  }

  inline int32_t GetTopic(int32_t idx) const {
    return token_topics_[idx];
  }

  inline int32_t GetNumTopics() const {
    return num_topics_;
  }

  // Print doc as "word:topic" pairs.
  std::string Print() {
    std::stringstream ss;
    for (LDADoc::Iterator it(this); !it.IsEnd(); it.Next()) {
      ss << it.Word() << ":" << it.Topic() << " ";
    }
    return ss.str();
  }

  void RandomInitTopics(int num_topics) {
    num_topics_ = num_topics;
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> one_K_rng(0, num_topics - 1);
    for (int i = 0; i < tokens_.size(); ++i) {
      token_topics_[i] = one_K_rng(gen);
    }
  }

  size_t SerializedSize() {
    // if num_topics = 0, then tokens and topics are uninitialized.
    // num_topics | num_tokens | tokens and topics
    if (num_topics_ == 0) {
      return sizeof(int32_t) + sizeof(size_t)
        + tokens_.size() * sizeof(int32_t);
    }
    return sizeof(int32_t) + sizeof(size_t)
      + tokens_.size() * 2 * sizeof(int32_t);
  }

  void Serialize(void *buf) const {
    uint8_t *buf_ptr = reinterpret_cast<uint8_t*>(buf);

    *(reinterpret_cast<int32_t*>(buf_ptr)) = num_topics_;
    buf_ptr += sizeof(int32_t);

    *(reinterpret_cast<size_t*>(buf_ptr)) = tokens_.size();
    buf_ptr += sizeof(size_t);

    memcpy(buf_ptr, tokens_.data(), tokens_.size() * sizeof(int32_t));
    buf_ptr += tokens_.size()*sizeof(int32_t);

    if (num_topics_ > 0) {
      memcpy(buf_ptr, token_topics_.data(),
          token_topics_.size() * sizeof(int32_t));
    }
  }

  void Deserialize(const void *buf) {
    const uint8_t *buf_ptr = reinterpret_cast<const uint8_t*>(buf);
    num_topics_ = *(reinterpret_cast<const int32_t*>(buf_ptr));
    buf_ptr += sizeof(int32_t);
    size_t num_tokens = *(reinterpret_cast<const size_t*>(buf_ptr));
    buf_ptr += sizeof(size_t);

    tokens_.resize(num_tokens);
    token_topics_.resize(num_tokens);
    memcpy(tokens_.data(), buf_ptr, num_tokens * sizeof(int32_t));
    buf_ptr += num_tokens * sizeof(int32_t);

    if (num_topics_ > 0) {
      memcpy(token_topics_.data(), buf_ptr, num_tokens * sizeof(int32_t));
    }
  }

  inline int GetNumTokens() {
    return tokens_.size();
  }

public:   // Iterator
  // Iterator lets you do this:
  //
  //  LDADoc doc;
  //  // ... Initialize it in some way ...
  //  for (LDADoc::Iterator it(&doc); !it.IsEnd(); it.Next()) {
  //    int word = it.Word();
  //    int topic = it.Topic();
  //    it.SetTopic(0);
  //  }
  //
  // Notice the is_end() in for loop. You can't use the == operator.
  class Iterator {
  public:
    // Does not take the ownership of doc.
    Iterator(LDADoc* doc) : doc_(*doc), curr_(0) { }

    inline int Word() {
      //CHECK_LT(curr_, doc_.tokens_.size());
      return doc_.tokens_[curr_];
    }

    inline int Topic() {
      //CHECK_LT(curr_, doc_.tokens_.size());
      return doc_.token_topics_[curr_];
    }

    inline void Next() {
      ++curr_;
    }

    inline bool IsEnd() {
      return curr_ == doc_.tokens_.size();
    }

    inline void SetTopic(int new_topic) {
      doc_.token_topics_[curr_] = new_topic;
    }

  private:
    LDADoc& doc_;

    // The index to the tokens_ and token_topics_.
    int32_t curr_;
  };

private:
  // tokens_[i] has topic token_topics_[i].
  std::vector<int32_t> tokens_;
  std::vector<int32_t> token_topics_;
  int32_t num_topics_;
};


}  // namespace lda

#endif    // LDA_LDA_DOC
