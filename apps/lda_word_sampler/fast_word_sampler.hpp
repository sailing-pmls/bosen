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

#pragma once

#include <petuum_ps/include/petuum_ps.hpp>
#include "types.hpp"
#include "topic_counts.hpp"
#include <list>
#include <vector>

namespace lda {

// WordSampler samples all occurrences of a word across all documents.
class WordSampler {
  public:
    // Default constructor needed for STL container.
    WordSampler();
    ~WordSampler();

    // Init tables and set size of topic_counts_. WordSampler does not take
    // ownership of any CountTable.
    void Init(int32_t word);

    // Add a doc with token (word_) counts.
    void AddDocTokens(int32_t doc_id, int32_t count);

    // Initialize topics for all occurrence of word_ by uniform random on
    // 0~(K-1).
    void InitTopicsUniform();

    void Sample(rng_t *my_rng);

    // Compute the contribution of a word to the log likelihood.
    double ComputeWordLikelihood();

    // Get number of tokens in this word_.
    int32_t GetNumTokens();

    // Return a pair of word and count
    wcmap_t GetWordCountMap(int topic);

    // word topic:count topic:count ...
    std::string Print();

  private:
    void FastGibbsSampling(int32_t doc_id,
        std::vector<int32_t> &topic_assignments,
        rng_t *rng);

  private:
    // The word being sampled.
    int32_t word_;

    int32_t num_topics_;
    double alpha_;
    double beta_;
    double beta_sum_;

    // Unnormalized distribution (counts) over topics for word_ (i.e., a
    // column in topic-word table.) topic_counts_[k] is the number of
    // word_ assigned to topic k.
    TopicCounts topic_counts_;

    // doc id => latent topic assignments of word_ appearing in the doc
    typedef std::pair<int32_t, std::vector<int32_t> > DocTopicAssingments;

    // Latent topic assignment z associated with each occurrence of word_
    // across all documents.
    std::list<DocTopicAssingments> z_;

    // See comments in lda_sampler.hpp
    petuum::Table<int> summary_table_;
    petuum::Table<int> doc_topic_table_;

    // Number of tokens which are word_.
    int32_t num_tokens_;

    int32_t *sum_row_cache_;
    int32_t *doc_topic_cache_;

    // Fast sampler
    double Abar;
    double Bbar;
    double *Ccoeff;
    double *Cval;

    // XY fast sampler
    double Xbar;
    double *Ycoeff;
    double *Yval;
    int *flat_topic_counts;
};

}   // namespace lda
