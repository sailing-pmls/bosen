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

#include "fast_word_sampler.hpp"
#include "context.hpp"
#include "utils.hpp"

#include <glog/logging.h>

#include <time.h>
#include <stdlib.h>
#include <sstream>
#include <algorithm>

namespace lda {

WordSampler::WordSampler() :
    sum_table_(NULL),
    doc_topic_table_(NULL),
    num_tokens_(0) {
  Context& context = Context::get_instance();
  int32_t num_topics = context.get_int32("num_topics");
  double alpha = context.get_double("alpha");
  double beta = context.get_double("beta");

  num_topics_ = num_topics;
  alpha_ = alpha;
  beta_ = beta;

  Ccoeff = new double[num_topics_];
  Cval = new double[num_topics_];
  sum_row_cache_ = new int32_t[num_topics_];
  doc_topic_cache_ = new int32_t[num_topics_];

  // XY
  Ycoeff = new double[num_topics_];
  Yval = new double[num_topics_];
  flat_topic_counts = new int[num_topics_];
}

WordSampler::~WordSampler() {
  delete[] Ccoeff;
  delete[] Cval;
  delete[] sum_row_cache_;
  delete[] doc_topic_cache_;

  // XY
  delete[] Ycoeff;
  delete[] Yval;
  delete[] flat_topic_counts;
}

void WordSampler::Init(CountTable *sum_table, CountTable *doc_topic_table,
      int32_t word) {
  word_ = word;
  sum_table_ = sum_table;
  doc_topic_table_ = doc_topic_table;
}

void WordSampler::AddDocTokens(int32_t doc_id, int32_t count) {
  z_.push_back(std::make_pair(doc_id, std::vector<int32_t>(count)));
  num_tokens_ += count;
}

void WordSampler::InitTopicsUniform() {
  srand((unsigned)time(NULL));
  for (auto& doc_tokens : z_) {
    int32_t doc_id = doc_tokens.first;
    // Iterate over $count topic assignments
    for (auto& token_topic : doc_tokens.second) {
      token_topic = rand() % num_topics_;
      doc_topic_table_->Inc(doc_id, token_topic, 1);
      // NOTE(xun): the only row id of sum_table_ is set to 0
      sum_table_->Inc(0, token_topic, 1);
      if (!topic_counts_.findAndIncrement(token_topic))
        topic_counts_.addNewTopAftChk(token_topic);
    }
  } // end of for every doc topic pair
}

void WordSampler::Sample(rng_t *my_rng) {
  Context& context = Context::get_instance();
  double beta_sum = context.get_double("beta_sum");
  beta_sum_ = beta_sum;

  // Take a snapshot of sum row
  petuum::DenseRow<int>& sum_row_ref = sum_table_->GetRowUnsafe(0);
  for (int k = 0; k < num_topics_; ++k) {
    sum_row_cache_[k] = std::max(sum_row_ref[k], 0);
  }

  memset(flat_topic_counts, 0, sizeof(int) * num_topics_);
  memset(Ycoeff, 0, sizeof(double) * num_topics_);
  memset(Yval, 0, sizeof(double) * num_topics_);
  double Xbar = 0.0;

  int length = topic_counts_.length;
  cnt_topic_t *items = topic_counts_.items;
  register topic_t* topics = (topic_t*) items;
  cnt_t* cnts = (cnt_t*) (items) + 1;
  register topic_t *endTop = (topic_t*) (items + length - 1);
  for (; topics <= endTop; topics += 2) { // only non zeros
    register topic_t currentTopic = *topics;
    Ycoeff[currentTopic] = beta_ + (*cnts);
    flat_topic_counts[currentTopic] = *cnts;
    cnts += 2;
  }
  double *yc_index = Ycoeff;
  int *gt_index = sum_row_cache_;
  register double reg_denom_pre = 0;
  for (topic_t k = 0; k < num_topics_; ++k) {
    reg_denom_pre = *gt_index + beta_sum_;
    if (*yc_index == 0) { // zeros
      *yc_index += beta_;
    }
    *yc_index /= reg_denom_pre;
    Xbar += *yc_index;
    ++yc_index;
    ++gt_index;
  }
  Xbar *= alpha_;

  for (auto& doc_tokens : z_) {
    int32_t doc_id = doc_tokens.first;
    // Take a snapshot of this row
    petuum::DenseRow<int>& doc_topic_row_ref = doc_topic_table_->GetRowUnsafe(doc_id);
    for (int k = 0; k < num_topics_; ++k) {
      doc_topic_cache_[k] = std::max(doc_topic_row_ref[k], 0);
    }

    for (auto& old_topic : doc_tokens.second) {
      // Decrement
      register int reg_old_topic = old_topic;
      Xbar -= alpha_ * Ycoeff[reg_old_topic];
      --flat_topic_counts[reg_old_topic]; // pseudo decrement
      --doc_topic_cache_[reg_old_topic];
      --sum_row_cache_[reg_old_topic];
      Ycoeff[reg_old_topic] = 
          (beta_ + flat_topic_counts[reg_old_topic]) /
          (sum_row_cache_[reg_old_topic] + beta_sum_);
      Xbar += alpha_ * Ycoeff[reg_old_topic];

      // Compute Ybar
      register double Ybar = 0.0;
      int *dtc_index = doc_topic_cache_;
      register double *yv_index = Yval;
      register double *yc_index = Ycoeff;
      for (topic_t k = 0; k < num_topics_; ++k) {
        if (*dtc_index > 0) { // non zeros
          *yv_index = (*dtc_index) * (*yc_index);
          Ybar += *yv_index;
        }
        ++dtc_index;
        ++yv_index;
        ++yc_index;
      }

      // Do sample
      double mass = Xbar + Ybar;
      //double zero_one = ((double) rand() / (RAND_MAX));
      //double sample = zero_one * mass;
      double sample = (*my_rng)() * mass;
      topic_t new_topic = -1;
      if (sample < Ybar) {
        yv_index = Yval;
        new_topic = num_topics_ - 1;
        for (int idx = 0; idx < num_topics_ - 1; ++idx) {
          sample -= *(yv_index++);
          if (sample <= 0.0) {
            new_topic = idx;
            break;
          }
        }
      } else {
        sample -= Ybar;
        sample /= alpha_;
        yc_index = Ycoeff;
        new_topic = num_topics_ - 1;
        for (int idx = 0; idx < num_topics_ - 1; ++idx) {
          sample -= *(yc_index++);
          if (sample <= 0.0) {
            new_topic = idx;
            break;
          }
        }
      } // end of comparing with Ybar

      CHECK_GE(new_topic, 0);
      CHECK_LT(new_topic, num_topics_);

      // Increment
      register int reg_new_topic = new_topic;
      Xbar -= alpha_ * Ycoeff[reg_new_topic];
      ++flat_topic_counts[reg_new_topic]; // pseudo increment
      ++doc_topic_cache_[reg_new_topic];
      ++sum_row_cache_[reg_new_topic];
      Ycoeff[reg_new_topic] = 
          (beta_ + flat_topic_counts[reg_new_topic]) /
          (sum_row_cache_[reg_new_topic] + beta_sum_);
      Xbar += alpha_ * Ycoeff[reg_new_topic];

      // Set
      if (old_topic != new_topic) {
        topic_counts_.upd_count(old_topic, new_topic);
        doc_topic_table_->Inc(doc_id, old_topic, -1);
        doc_topic_table_->Inc(doc_id, new_topic, 1);
        sum_table_->Inc(0, old_topic, -1);
        sum_table_->Inc(0, new_topic, 1);
        old_topic = new_topic;
      }
    } // end of every token
  } // end of sampling in one doc
}

void WordSampler::FastGibbsSampling(int32_t doc_id,
    std::vector<int32_t> &topic_assignments,
    rng_t *rng) {
  double alpha_beta = alpha_ * beta_;
  for (auto& old_topic : topic_assignments) {
    // Decrement
    register int reg_old_topic = old_topic;
    register double reg_denom =
        sum_row_cache_[reg_old_topic] + beta_sum_;
    register int reg_dtc_old = doc_topic_cache_[reg_old_topic];
    Abar -= alpha_beta / reg_denom;
    Bbar -= beta_ * reg_dtc_old / reg_denom;
    doc_topic_cache_[reg_old_topic] = std::max(
        doc_topic_cache_[reg_old_topic] - 1, 0);
    reg_dtc_old = std::max(reg_dtc_old - 1, 0);
    sum_row_cache_[reg_old_topic] = std::max(
        sum_row_cache_[reg_old_topic] - 1, 0);
    reg_denom = std::max(reg_denom - 1, .0);
    Abar += alpha_beta / reg_denom;
    Bbar += beta_ * reg_dtc_old / reg_denom;
    Ccoeff[reg_old_topic] = (alpha_ + reg_dtc_old) / reg_denom;

    // Do sample
    register double Cbar = 0.0;
    int length = topic_counts_.length;
    cnt_topic_t *items = topic_counts_.items;
    register topic_t* topics = (topic_t*) items;
    cnt_t* cnts = (cnt_t*) (items) + 1;
    register topic_t *endTop = (topic_t*) (items + length - 1);
    register double *ttsptr = Cval;
    for (; topics <= endTop; topics += 2) {
      register topic_t currentTopic = *topics;
      *ttsptr = (currentTopic == reg_old_topic) ? 
          Ccoeff[currentTopic] * std::max(*cnts - 1, 0) :
          Ccoeff[currentTopic] * (*cnts);
      Cbar += *ttsptr;
      ++ttsptr;
      cnts += 2;
    }
    double mass = Abar + Bbar + Cbar;
    double sample = (*rng)() * mass;
    topic_t new_topic = -1;
    if (sample < Cbar) {
      new_topic = *endTop;
      ttsptr = Cval;
      topics = (topic_t*) items;
      for (; topics < endTop; topics += 2) {
        sample -= *(ttsptr++);
        if (sample <= 0.0) {
            new_topic = *topics;
            break;
        }
      }
    } else {
      sample -= Cbar;
      if (sample < Bbar) {
        sample /= beta_;
        new_topic = num_topics_ - 1;
        for (int idx = 0; idx < num_topics_ - 1; ++idx) {
          sample -= doc_topic_cache_[idx] / 
              (sum_row_cache_[idx] + beta_sum_);
          if (sample <= 0.0) {
            new_topic = idx;
            break;
          }
        }
      } else {
        sample -= Bbar;
        sample /= alpha_beta;
        new_topic = num_topics_ - 1;
        for (int idx = 0; idx < num_topics_ - 1; ++idx) {
          sample -= 1.0 / (sum_row_cache_[idx] + beta_sum_);
          if (sample <= 0.0) {
            new_topic = idx;
            break;
          }
        }
      } // end Bbar or Abar
    } // end of comparing with Cbar

    CHECK_GE(new_topic, 0);
    CHECK_LT(new_topic, num_topics_);

    // Increment
    register int reg_new_topic = new_topic;
    register int reg_dtc_new = doc_topic_cache_[reg_new_topic];
    reg_denom = sum_row_cache_[reg_new_topic] + beta_sum_;
    Abar -= alpha_beta / reg_denom;
    Bbar -= beta_ * reg_dtc_new / reg_denom;
    ++doc_topic_cache_[reg_new_topic];
    ++reg_dtc_new;
    ++sum_row_cache_[reg_new_topic];
    ++reg_denom;
    Abar += alpha_beta / reg_denom;
    Bbar += beta_ * reg_dtc_new / reg_denom;
    Ccoeff[reg_new_topic] = (alpha_ + reg_dtc_new) / reg_denom;

    // Set
    if (old_topic != new_topic) {
      topic_counts_.upd_count(old_topic, new_topic);
      doc_topic_table_->Inc(doc_id, old_topic, -1);
      doc_topic_table_->Inc(doc_id, new_topic, 1);
      sum_table_->Inc(0, old_topic, -1);
      sum_table_->Inc(0, new_topic, 1);
      old_topic = new_topic;
    }
  }
}

double WordSampler::ComputeWordLikelihood() {
  double word_log_likelihood = 0.0;
  int length = topic_counts_.length;
  for (int j = 0; j < length; ++j) {
    cnt_topic_t cnt_top = topic_counts_.items[j];
    int word_topic_count = cnt_top.choose.cnt;
    word_log_likelihood += LogGamma(word_topic_count + beta_);
  }
  // This can be removed if this function is solely used to compute delta.
  word_log_likelihood -= length * LogGamma(beta_);

  return word_log_likelihood;
}

int32_t WordSampler::GetNumTokens() {
  return num_tokens_;
}

wcmap_t WordSampler::GetWordCountMap(int topic) {
  wcmap_t temp;
  temp.word = word_;
  temp.count = topic_counts_.get_counts(topic);
  return temp;
}

std::string WordSampler::Print() {
  return std::to_string(word_) + topic_counts_.print() + "\n";
}

}   // namespace lda
