// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.04.03

#include "lda_stats.hpp"
#include "context.hpp"
#include "utils.hpp"
#include <vector>
#include <glog/logging.h>
#include <string>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace lda {

LDAStats::LDAStats() {
  // Topic model parameters.
  util::Context& context = util::Context::get_instance();
  K_ = context.get_int32("num_topics");
  V_ = context.get_int32("num_vocabs");
  CHECK_NE(-1, V_);
  beta_ = context.get_double("beta");
  beta_sum_ = beta_ * V_;
  alpha_ = context.get_double("alpha");
  alpha_sum_ = K_ * alpha_;

  // Precompute LLH parameters
  log_doc_normalizer_ = LogGamma(alpha_sum_) - K_ * LogGamma(alpha_);
  log_topic_normalizer_ = LogGamma(beta_sum_) - V_ * LogGamma(beta_);

  // PS tables.
  int32_t summary_table_id = context.get_int32("summary_table_id");
  int32_t word_topic_table_id = context.get_int32("word_topic_table_id");
  int32_t llh_table_id = context.get_int32("llh_table_id");
  summary_table_ = petuum::TableGroup::GetTableOrDie<int>(summary_table_id);
  word_topic_table_ = petuum::TableGroup::GetTableOrDie<int>(
      word_topic_table_id);
  llh_table_ = petuum::TableGroup::GetTableOrDie<double>(llh_table_id);
}

void LDAStats::ComputeOneDocLLH(int32_t ith_llh, LDADocument* doc) {
  double one_doc_llh = log_doc_normalizer_;

  // Compute doc-topic vector on the fly.
  std::vector<int32_t> doc_topic_vec(K_);
  std::fill(doc_topic_vec.begin(), doc_topic_vec.end(), 0);
  int num_words = 0;
  for (LDADocument::WordOccurrenceIterator it(doc);
      !it.Done(); it.Next()) {
    ++doc_topic_vec[it.Topic()];
    ++num_words;
  }
  for (int k = 0; k < K_; ++k) {
    CHECK_LE(0, doc_topic_vec[k]) << "negative doc_topic_vec[k]";
    one_doc_llh += LogGamma(doc_topic_vec[k] + alpha_);
  }
  one_doc_llh -= LogGamma(num_words + alpha_sum_);
  CHECK_EQ(one_doc_llh, one_doc_llh) << "one_doc_llh is nan.";
  llh_table_.Inc(ith_llh, 1, one_doc_llh);
}

void LDAStats::ComputeWordLLH(int32_t ith_llh, int32_t iter) {
  // word_llh is P(w|z).
  double word_llh = K_ * log_topic_normalizer_;
  double zero_entry_llh = LogGamma(beta_);
  // Since some vocabs are not present in the corpus, use num_words_seen to
  // count # of words in corpus.
  int num_words_seen = 0;
  for (int w = 0; num_words_seen < V_ && w < 2 * V_; ++w) {
    petuum::RowAccessor word_topic_row_acc;
    word_topic_table_.Get(w, &word_topic_row_acc);
    const auto& word_topic_row =
      word_topic_row_acc.Get<petuum::SortedVectorMapRow<int32_t> >();
    if (word_topic_row.num_entries() > 0) {
      ++num_words_seen;  // increment only when word has non-zero tokens.
      for (petuum::SortedVectorMapRow<int>::const_iterator wt_it =
          word_topic_row.cbegin(); !wt_it.is_end(); ++wt_it) {
        int count = wt_it->second;
        CHECK_LE(0, count) << "negative count.";
        word_llh += LogGamma(count + beta_);
        CHECK_EQ(word_llh, word_llh)
          << "word_llh is nan after LogGamma(count + beta_). count = "
          << count;
      }
      // The other word-topic counts are 0.
      int num_zeros = K_ - word_topic_row.num_entries();
      word_llh += num_zeros * zero_entry_llh;
    }
  }

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
  llh_table_.Inc(ith_llh, 1, word_llh);

  // Since only 1 client should call this ComputeWordLLH, we set the first
  // column to be iter-#
  llh_table_.Inc(ith_llh, 0, static_cast<double>(iter));
}

std::string LDAStats::PrintLLH(int32_t num_llh) {
  std::stringstream output;
  for (int i = 1; i <= num_llh; ++i) {
    petuum::RowAccessor llh_row_acc;
    llh_table_.Get(i, &llh_row_acc);
    const auto& llh_row = llh_row_acc.Get<petuum::DenseRow<double> >();
    output << llh_row[0] << " " << llh_row[1] << std::endl;
  }
  return output.str();
}

std::string LDAStats::PrintOneLLH(int32_t ith_llh) {
  std::stringstream output;
  petuum::RowAccessor llh_row_acc;
  llh_table_.Get(ith_llh, &llh_row_acc);
  const auto& llh_row = llh_row_acc.Get<petuum::DenseRow<double> >();
  output << llh_row[0] << " " << llh_row[1] << std::endl;
  return output.str();
}

}  // namespace lda
