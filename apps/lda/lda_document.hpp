// Copyright 2008 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

//#include <stdio.h>
#include <cstdlib>
#include <vector>
#include <cstdint>
#include <list>
#include <string>

namespace lda {

// The structure representing a document as a bag of words and the
// topic assigned to each occurrence of a word.  In term of Bayesian
// learning and LDA, the bag of words are ``observable'' data; the
// topic assignments are ``hidden'' data.
struct DocumentWordTopicsPB {
  // The document unique words list.
  std::vector<int32_t> words_;

  // Each word occurrance's topic.
  //  wordtopics_.size() = num_words_in_document.
  //  words_.size() = num_unique_words_in_document.
  std::vector<int32_t> wordtopics_;

  // A map from words_ to wordtopics_.
  // For example:
  // The document: WORDS1:4  WORD2:2 WORD3:1
  // words_:                     WORD1 WORD2  WORD3
  // wordtopics_start_index_:     |       \      |
  // wordtopics_:                 0 3 4 0  0 3   1
  std::vector<int32_t> wordtopics_start_index_;
  DocumentWordTopicsPB() { wordtopics_start_index_.push_back(0); }

  // Number of unique words in the doc.
  int32_t words_size() const { return words_.size(); }

  // Number of occurrence of word word_index.
  int32_t wordtopics_count(int32_t word_index) const {
    return wordtopics_start_index_[word_index + 1] -
      wordtopics_start_index_[word_index];
  }
  int32_t word_last_topic_index(int32_t word_index) const {
    return wordtopics_start_index_[word_index + 1] - 1;
  }
  int32_t word(int32_t word_index) const { return words_[word_index]; }
  int32_t wordtopics(int32_t index) const { return wordtopics_[index]; }
  int32_t* mutable_wordtopics(int32_t index) { return &wordtopics_[index]; }

  void add_wordtopics(int32_t word, const std::vector<int32_t>& topics) {
    words_.push_back(word);
    wordtopics_start_index_.pop_back();
    wordtopics_start_index_.push_back(wordtopics_.size());
    for (size_t i = 0; i < topics.size(); ++i) {
      wordtopics_.push_back(topics[i]);
    }
    wordtopics_start_index_.push_back(wordtopics_.size());
  }
  void CopyFrom(const DocumentWordTopicsPB& instance) { *this = instance; }
};


// Stores a document as a bag of words and provides methods for interacting
// with Gibbs LDA models. It does not store the document-topic vector but for
// space and the cost of maintaining sparse data structure reasons, the
// sampler computes it on the fly, before sampling this document.
class LDADocument {
 public:
  // An iterator over all of the word occurrences in a document.
  class WordOccurrenceIterator {
   public:
    // Intialize the WordOccurrenceIterator for a document.
    explicit WordOccurrenceIterator(LDADocument* parent);
    ~WordOccurrenceIterator();

    // Returns true if we are done iterating.
    bool Done();

    // Advances to the next word occurrence.
    void Next();

    // Returns the topic of the current occurrence.
    int Topic();

    // Changes the topic of the current occurrence.
    void SetTopic(int new_topic);

    // Returns the word of the current occurrence.
    int Word();

    // Reset word_index_
    void GotoWord(int new_word_index);

   private:
    // If the current word has no occurrences, advance until reaching a word
    // that does have occurrences or the end of the document.
    void SkipWordsWithoutOccurrences();

    LDADocument* parent_;
    int word_index_;
    int word_topic_index_;
  };
  friend class WordOccurrenceIterator;

  // Initializes a document from a DocumentWordTopicsPB. Usually, this
  // constructor is used in training an LDA model, because the
  // initialization phase creates the model whose vocabulary covers
  // all words appear in the training data.
  LDADocument(const DocumentWordTopicsPB& topics);

  virtual ~LDADocument();

  // Returns the document's topic associations.
  const DocumentWordTopicsPB& topics() const {
    return *topic_assignments_;
  }

  // Returns the document's topic occurrence counts.
  //const vector<int64>& topic_distribution() const {
  //  return topic_distribution_;
  //}

  //void ResetWordIndex(const map<string, int>& word_index_map);

  std::string DebugString();
 protected:
  DocumentWordTopicsPB*  topic_assignments_;
  //vector<int64> topic_distribution_;

  // Count topic occurrences in topic_assignments_ and stores the
  // result in topic_distribution_.
  //void CountTopicDistribution();
};

typedef std::list<LDADocument> LDACorpus;

}  // namespace lda
