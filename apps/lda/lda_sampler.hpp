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

#include "types.hpp"
#include "fast_word_sampler.hpp"

#ifdef LOCAL_SCHED
#include "local_scheduler/local_scheduler.hpp"
#endif

#include <boost/unordered_map.hpp>
#include <boost/thread/tss.hpp>
#include <boost/thread.hpp>
#include <atomic>

namespace lda {

// A coordinator of word-based Gibbs sampler. LocalScheduler produces words to
// sample, sampler thread takes one word (id) from candidate queue, pick
// corresponding word sampler from the pool, execute, and push the value to
// finished queue to notify the scheduler.
class LDASampler {
  public:
    LDASampler(petuum::TableGroup<int32_t>* table_group,
               CountTable* summary_row,
               CountTable* doc_topic_table);
               //CountTable* likelihood_table);

    // Read word stat data. This should only be called once by one thread.
    void ReadData(bool zero_indexed = true);

    // RunSampler() is concurrent. It first InitTopic() in parallel, reach a
    // barrier until all threads are finished, and then SampleOneIteration() in
    // parallel. It runs until processing (num_iterations * num_threads *
    // num_words_per_thread_per_iteration) words, where the last parameter is
    // user defined.
    void RunSampler();

  private:    // private functions.
    // Initialize parameters and variables with uniform random sampling. This
    // method is concurrent. Returns the number of words (vocabs) initialized. 
    int32_t InitTopics();

    // Gibbs sampler
    void SampleOneIteration();

    // Compute ln P(w | z) and ln P(z) in parallel respectively. Both start with
    // a process barrier that blocks all threads. Note that ln P(w | z) is only
    // computed on local words. 
    // TODO(xun): how to execute the latter one only on one machine? plus gather
    // the word likelihood. MPI_Reduce?!
    void ComputeLocalWordLikelihood();
    void ComputeDocLikelihood();

    void PrintTopWords();

  private:  // private variables.
    // Number of iterations to run.
    int32_t num_iterations_;

#ifdef LOCAL_SCHED
    // Number of words (vocabs) per thread per iteration.
    int32_t num_words_per_thread_per_iteration_;
#endif

    // Compute log likelihood on every N iterations
    int32_t compute_ll_interval_;

    // Handler of everything in petuum
    petuum::TableGroup<int32_t> *table_group_;

    // Summary column of topic-word table.
    // #row = 1, #column = num_topics.
    CountTable *summary_row_;

    // Every row represents a document's topic vector.
    // #row = num_docs, #column = num_topics.
    CountTable *doc_topic_table_;

    // Likelihood table. Note that it's integer type..
    //CountTable *likelihood_table_;

    // LDASampler holds all word samplers in the local machine.
    boost::unordered_map<int32_t, WordSampler> word_samplers_;
    boost::unordered_map<int32_t, WordSampler>::iterator word_iterator_;

    // Mutex for init topics in parallel.
    boost::mutex word_lock_;

#ifdef LOCAL_SCHED
    // LocalScheduler
    boost::scoped_ptr<petuum::LocalScheduler> scheduler_;
#endif

    // Track number of words (vocabs) initialized by InitTopic().
    std::atomic<int32_t> num_words_initialized_;

    // Local barrier across threads.
    boost::scoped_ptr<boost::barrier> process_barrier_;

    // Data that a thread needs to do sampling.
    struct LDASamplerThreadData {
      int32_t thread_id;
      boost::mt19937 generator_;
      boost::uniform_real<double> uniform_zero_one_dist_;
      boost::scoped_ptr<rng_t> zero_one_generator_;

      LDASamplerThreadData();
    };

    // Each thread gets its own LDASamplerThreadData.
    boost::thread_specific_ptr<LDASamplerThreadData> sampler_thread_data_;

    // Used to get thread_id for each thread.
    std::atomic<int32_t> thread_counter_;

    // Used to keep track of average delta value, so that we can generate
    // moderately small delta in special cases like delta is negative.
    int32_t num_words_sampled_;
    double average_delta_;
    boost::mutex average_delta_lock_;

    // Used to compute word likelihood.
    double word_likelihood_;
    boost::mutex word_likelihood_lock_;

    // Used to compute doc likelihood
    int32_t doc_iterator_;
    boost::mutex doc_lock_;
    double doc_likelihood_;
    boost::mutex doc_likelihood_lock_;

    // Number of tokens sampled in an iteration.
    std::atomic<int32_t> num_tokens_iteration_;

    // A map of word_id and literal words
    std::vector<std::string> dict_;
};

}   // namespace lda
