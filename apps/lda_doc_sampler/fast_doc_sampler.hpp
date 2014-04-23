// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.03.28

#pragma once

#include "lda_document.hpp"
#include <petuum_ps/include/petuum_ps.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/random/variate_generator.hpp>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace lda {

typedef double real_t;

// FastDocSampler samples one document at a time using Yao's fast sampler
// (http://people.cs.umass.edu/~lmyao/papers/fast-topic-model10.pdf section
// 3.4. We try to keep the variable names consistent with the paper.)
// FastDocSampler does not store any documents.
class FastDocSampler {
public:
  FastDocSampler();
  ~FastDocSampler();

  // Read off summary_table and cache it to summary_row_ to simulate thread
  // cache (avoiding contention). Write the delta accuulated in
  // summary_row_delta_ to summary_table_ and zero out summary_row_delta_.
  void RefreshCachedSummaryRow();

  // RefreshCachedSummaryRow must be called at least once before calling
  // SampleOneDoc.
  void SampleOneDoc(LDADocument* doc);

private:  // private functions
  // Since we don't store doc-topic vector for each document, we compute it
  // before sampling a doc and store it to doc_topic_vec_.
  void ComputeDocTopicVector(LDADocument* doc);

  // Compute the following fast sampler auxiliary variables:
  //
  // 1. mass in s bucket and store in s_sum_.
  // 2. mass in r bucket and store in r_sum_.
  // 3. coefficients in the q term and store in q_coeff_.
  // 4. populate nonzero_doc_topic_idx_ based on doc_topic_vec_.
  // 5. populate summary_row_.
  //
  // This requires that doc_topic_vec_ is already computed.
  void ComputeAuxVariables();

  int32_t Sample();

private:  // private members.
  // ================ Topic Model Parameters =================
  // Number of topics.
  int32_t K_;

  // Number of vocabs.
  int32_t V_;

  // Dirichlet prior for word-topic vectors.
  real_t beta_;

  // Equals to V_*beta_
  real_t beta_sum_;

  // Dirichlet prior for doc-topic vectors.
  real_t alpha_;

  // ============== Global Parameters from Petuum Server =================
  // A table containing just one summary row of [K x 1] dimension. The k-th
  // entry in the table is the # of tokens assigned to topic k.
  petuum::Table<int32_t> summary_table_;

  // Word topic table of V_ rows, each of which is a [K x 1] dim sparse sorted
  // row.
  petuum::Table<int32_t> word_topic_table_;

  // ============== Fast Sampler (Cached) Variables ================

  // The mass of s bucket in the paper, recomputed before sampling a document.
  //
  // Comment(wdai): In principle this is independent of a document, and only
  // needs to be recomputed when summary row is refreshed. However, since it's
  // cumbersome interface to let the sampler know when summary row is
  // refreshed, we recompute this at the beginning of each document (very
  // cheap).
  //
  // Comment(wdai): We do not maintain each term in the sum because for small
  // alpha_ and beta_, the chance of a sample falling into this bucket is so
  // small (<10%) that it might be cheaper to compute the necessary terms on
  // the fly when this bucket is picked instead of maintaining the vector of
  // terms.
  real_t s_sum_;

  // The mass of r bucket in the paper. This needs to be computed for each
  // document, and updated incrementally as each token in the doc is sampled.
  //
  // Comment(wdai): We don't maintain each terms in the sum for the same
  // reasons as s_sum_.
  real_t r_sum_;

  // The mass of q bucket in the paper. This is computed for each word with
  // the help of q_coeff_.
  real_t q_sum_;

  // The cached coefficients in the q bucket. The coefficients are the terms
  // without word-topic count. [K_ x 1] dimension. This is computed before
  // sampling of each document, and incrementally updated.
  std::vector<real_t> q_coeff_;

  // Nonzero terms in the summand of q bucket. [K_ x 1] dimension, but only
  // the first num_nonzero_q_terms_ enetries are active.
  std::vector<real_t> nonzero_q_terms_;

  // The corresponding topic index in nonzero_q_terms_. [K_ x 1] dimension.
  std::vector<real_t> nonzero_q_terms_topic_;

  // q_terms_ is sparse as word-topic count is sparse (0 for most topics).
  int32_t num_nonzero_q_terms_;

  // Compute doc_topic_vec_ before sampling a document. This remove the need
  // to store the potentially big doc-topic vector in each document, and
  // shouldn't add much runtime overhead.
  //
  // Comment(wdai): doc_topic_vec_ is sparse with large K_. If we use
  // memory-efficient way, e.g. map, to represent it the access time would be
  // high.
  std::vector<int32_t> doc_topic_vec_;

  // Use a local copy of summary_row to allow transient -1 when taking a token
  // out, which we will do batch write to PS later. Dimension = [K x 1]. This
  // also reduce the time we hold accessor (a lock) on the summary row.
  std::vector<int32_t> summary_row_;

  // Keep track of deltas that are not written to summary_table_.
  std::vector<int32_t> summary_row_delta_;

  // Sorted indices of nonzero topics in doc_topic_vec_ to exploit sparsity in
  // doc_topic_vec. Sorted so we can use std::lower_bound binary search. This
  // has size [K x 1], but only the first num_nonzero_idx_ entries are active.
  //
  // Comment(wdai): Use POD array to allow memmove, which is faster than using
  // std::vector with std::move.
  int32_t* nonzero_doc_topic_idx_;

  // Number of non-zero entries in doc_topic_vec_.
  int32_t num_nonzero_doc_topic_idx_;

  // ================== Utilities ====================
  typedef boost::variate_generator<boost::mt19937&,
          boost::uniform_real<real_t> > rng_t;

  boost::mt19937 rng_engine_;

  boost::uniform_real<real_t> uniform_zero_one_dist_;

  // zero_one_rng_ generates random real on [0, 1)
  std::unique_ptr<rng_t> zero_one_rng_;
};

}  // namespace lda
