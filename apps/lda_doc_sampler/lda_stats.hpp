// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.04.03

#pragma once

#include "lda_document.hpp"
#include <petuum_ps/include/petuum_ps.hpp>
#include <string>

namespace lda {


// We reference Griffiths and Steyvers (PNAS, 2003).
class LDAStats {
public:
  LDAStats();

  // The i-th complete-llh calculation will use row i in llh_able_. This is
  // part of log P(z) in eq.[3].
  void ComputeOneDocLLH(int32_t ith_llh, LDADocument* doc);

  // This computes the entire log P(w|z) in eq.[2].
  // compute_ll_interval * ith_llh = iter 
  void ComputeWordLLH(int32_t ith_llh, int32_t iter);

  // Return a string of two columns: "iter-# llh."
  std::string PrintLLH(int32_t num_llh);

private:
  // ================ Topic Model Parameters =================
  // Number of topics.
  int32_t K_;

  // Number of vocabs.
  int32_t V_;

  // Dirichlet prior for word-topic vectors.
  float beta_;

  // V_*beta_
  float beta_sum_;

  // Dirichlet prior for doc-topic vectors.
  float alpha_;

  // K_*alpha__
  float alpha_sum_;

  // ================ Precompute LLH Parameters =================
  // Log of normalization constant (per docoument) from eq.[3].
  double log_doc_normalizer_;

  // Log of normalization constant (per topic) from eq.[2].
  double log_topic_normalizer_;

  // ============== Global Parameters from Petuum Server =================
  // A table containing just one summary row of [K x 1] dimension. The k-th
  // entry in the table is the # of tokens assigned to topic k.
  petuum::Table<int32_t> summary_table_;

  // Word topic table of V_ rows, each of which is a [K x 1] dim sparse sorted
  // row.
  petuum::Table<int32_t> word_topic_table_;

  // Log-likelihood table. Each row (say row i) has only one column storing
  // the complete log-likelihood of the i-th likelihood computation.
  petuum::Table<double> llh_table_;
};

}  // namespace lda
