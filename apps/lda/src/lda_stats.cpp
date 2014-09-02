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
// Date: 2014.04.03

#include "lda_stats.hpp"
#include "context.hpp"
#include "table_columns.hpp"
#include "utils.hpp"
#include <vector>
#include <glog/logging.h>
#include <string>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace lda {

const int LDAStats::kNumLogGammaAlpha_ = 100000;
const int LDAStats::kNumLogGammaAlphaSum_ = 10000;
const int LDAStats::kNumLogGammaBeta_ = 1000000;

LDAStats::LDAStats() {
  // Topic model parameters.
  Context& context = Context::get_instance();
  K_ = context.get_int32("num_topics");
  V_ = context.get_int32("num_vocabs");
  CHECK_NE(-1, V_);
  beta_ = context.get_double("beta");
  beta_sum_ = beta_ * V_;
  alpha_ = context.get_double("alpha");
  alpha_sum_ = K_ * alpha_;

  loggamma_alpha_offset_.resize(kNumLogGammaAlpha_);
  loggamma_alpha_sum_offset_.resize(kNumLogGammaAlphaSum_);
  loggamma_beta_offset_.resize(kNumLogGammaBeta_);
  for (int i = 0; i < kNumLogGammaAlpha_; ++i) {
    loggamma_alpha_offset_[i] = LogGamma(i + alpha_);
  }
  for (int i = 0; i < kNumLogGammaAlphaSum_; ++i) {
    loggamma_alpha_sum_offset_[i] = LogGamma(i + alpha_sum_);
  }
  for (int i = 0; i < kNumLogGammaBeta_; ++i) {
    loggamma_beta_offset_[i] = LogGamma(i + beta_);
  }

  // Precompute LLH parameters
  log_doc_normalizer_ = LogGamma(alpha_sum_) - K_ * LogGamma(alpha_);
  log_topic_normalizer_ = K_ * (LogGamma(beta_sum_) - V_ * LogGamma(beta_));

  // PS tables.
  int32_t summary_table_id = context.get_int32("summary_table_id");
  int32_t word_topic_table_id = context.get_int32("word_topic_table_id");
  int32_t llh_table_id = context.get_int32("llh_table_id");
  summary_table_ = petuum::PSTableGroup::GetTableOrDie<int>(summary_table_id);
  word_topic_table_ = petuum::PSTableGroup::GetTableOrDie<int>(
      word_topic_table_id);
  llh_table_ = petuum::PSTableGroup::GetTableOrDie<double>(llh_table_id);
}

double LDAStats::GetLogGammaAlphaOffset(int val) {
  if (val < kNumLogGammaAlpha_) {
    return loggamma_alpha_offset_[val];
  }
  return LogGamma(val + alpha_);
}

double LDAStats::GetLogGammaAlphaSumOffset(int val) {
  if (val < kNumLogGammaAlphaSum_) {
    return loggamma_alpha_sum_offset_[val];
  }
  return LogGamma(val + alpha_sum_);
}

double LDAStats::GetLogGammaBetaOffset(int val) {
  if (val < kNumLogGammaBeta_) {
    return loggamma_beta_offset_[val];
  }
  return LogGamma(val + beta_);
}

double LDAStats::ComputeOneDocLLH(LDADoc* doc) {
  double one_doc_llh = log_doc_normalizer_;

  // Compute doc-topic vector on the fly.
  std::vector<int32_t> doc_topic_vec(K_);
  std::fill(doc_topic_vec.begin(), doc_topic_vec.end(), 0);
  int num_words = 0;
  for (LDADoc::Iterator it(doc); !it.IsEnd(); it.Next()) {
    ++doc_topic_vec[it.Topic()];
    ++num_words;
  }
  for (int k = 0; k < K_; ++k) {
    CHECK_LE(0, doc_topic_vec[k]) << "negative doc_topic_vec[k]";
    //one_doc_llh += LogGamma(doc_topic_vec[k] + alpha_);
    one_doc_llh += GetLogGammaAlphaOffset(doc_topic_vec[k]);
  }
  //one_doc_llh -= LogGamma(num_words + alpha_sum_);
  one_doc_llh -= GetLogGammaAlphaSumOffset(num_words);
  CHECK_EQ(one_doc_llh, one_doc_llh) << "one_doc_llh is nan.";
  CHECK_GE(0, one_doc_llh) << "LLH of a doc cannot be positive.";
  return one_doc_llh;
}

void LDAStats::ComputeWordLLH(int32_t ith_llh, int word_idx_start,
    int word_idx_end) {
  double word_llh = 0.;
  static double zero_entry_llh = GetLogGammaBetaOffset(0);
  for (int w = word_idx_start; w < word_idx_end; ++w) {
    petuum::RowAccessor word_topic_row_acc;
    word_topic_table_.Get(w, &word_topic_row_acc);
    const auto& word_topic_row =
      word_topic_row_acc.Get<petuum::SortedVectorMapRow<int32_t> >();
    if (word_topic_row.num_entries() > 0) {
      for (petuum::SortedVectorMapRow<int>::const_iterator wt_it =
          word_topic_row.cbegin(); !wt_it.is_end(); ++wt_it) {
        int count = wt_it->second;
        CHECK_LE(0, count) << "negative count.";
        //word_llh += LogGamma(count + beta_);
        word_llh += GetLogGammaBetaOffset(count);
        CHECK_EQ(word_llh, word_llh)
          << "word_llh is nan after LogGamma(count + beta_). count = "
          << count;
      }
      // The other word-topic counts are 0.
      int num_zeros = K_ - word_topic_row.num_entries();
      word_llh += num_zeros * zero_entry_llh;
    }
  }
  CHECK_EQ(word_llh, word_llh) << "word_llh is nan.";
  llh_table_.Inc(ith_llh, kColIdxLLHTableLLH, word_llh);
}

void LDAStats::ComputeWordLLHSummary(int32_t ith_llh, int iter) {
  double word_llh = log_topic_normalizer_;
  // log(\prod_j (1 / \gamma(n_j^* + W\beta))) term.
  petuum::RowAccessor summary_row_acc;
  summary_table_.Get(0, &summary_row_acc);
  const auto& summary_row = summary_row_acc.Get<petuum::DenseRow<int32_t> >();
  for (int k = 0; k < K_; ++k) {
    word_llh -= LogGamma(summary_row[k] + beta_sum_);
    CHECK_EQ(word_llh, word_llh)
      << "word_llh is nan after -LogGamma(summary_row[k] + beta_). "
      << "summary_row[k] = " << summary_row[k];
  }
  CHECK_EQ(word_llh, word_llh) << "word_llh is nan.";
  llh_table_.Inc(ith_llh, kColIdxLLHTableLLH, word_llh);

  // Since only 1 client should call this ComputeWordLLH, we set the first
  // column to be iter-#
  llh_table_.Inc(ith_llh, kColIdxLLHTableIter, static_cast<double>(iter));
}

void LDAStats::SetTime(int32_t ith_llh, float time_in_sec) {
  llh_table_.Inc(ith_llh, kColIdxLLHTableTime, time_in_sec);
}

std::string LDAStats::PrintLLH(int32_t num_llh) {
  std::stringstream output;
  for (int i = 1; i <= num_llh; ++i) {
    petuum::RowAccessor llh_row_acc;
    llh_table_.Get(i, &llh_row_acc);
    const auto& llh_row = llh_row_acc.Get<petuum::DenseRow<double> >();
    output << llh_row[kColIdxLLHTableIter] << " "
      << llh_row[kColIdxLLHTableLLH] << " "
      << llh_row[kColIdxLLHTableTime] << std::endl;
  }
  return output.str();
}

std::string LDAStats::PrintOneLLH(int32_t ith_llh) {
  std::stringstream output;
  petuum::RowAccessor llh_row_acc;
  llh_table_.Get(ith_llh, &llh_row_acc);
  const auto& llh_row = llh_row_acc.Get<petuum::DenseRow<double> >();
  output << llh_row[kColIdxLLHTableIter] << " "
    << llh_row[kColIdxLLHTableLLH] << " "
    << llh_row[kColIdxLLHTableTime] << std::endl;
  return output.str();
}

}  // namespace lda
