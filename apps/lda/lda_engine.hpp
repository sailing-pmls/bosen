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
// Date: 2014.03.29

#pragma once

#include "lda_document.hpp"
#include "fast_doc_sampler.hpp"
#include "context.hpp"
#include <petuum_ps/include/petuum_ps.hpp>
#include <cstdint>
#include <boost/thread.hpp>
#include <boost/thread/tss.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>
#include <utility>
#include <vector>

namespace lda {

// Engine takes care of the entire pipeline of LDA, from reading data to
// spawning threads, to recording loglikelihood.
class LDAEngine {
public:
  LDAEngine();

  // Read word stat data. This should only be called once by one thread.
  void ReadData(const std::string& doc_file);

  // Concurrent.
  void Start(bool init_thread);

private:  // private functions
  // Randomly generate a topic on {1,..., K_} and add to doc_word_topics.
  void AddWordTopics(DocumentWordTopicsPB* doc_word_topics, int32_t word,
      int32_t num_tokens);

private:  // private data
  int32_t K_;   // number of topics

  // Number of application threads to be exact.
  int32_t num_threads_;

  // Local barrier across threads.
  //std::unique_ptr<boost::barrier> process_barrier_;

  std::vector<LDACorpus> thread_docs_;

  std::atomic<int32_t> thread_counter_;

  // Data that a thread needs to do sampling.
  struct LDAEngineThreadData {
    int32_t thread_id_;
    FastDocSampler sampler_;

    LDAEngineThreadData(int32_t thread_id) : thread_id_(thread_id) { }
  };

  // Each thread gets its own LDAEngineThreadData.
  boost::thread_specific_ptr<LDAEngineThreadData> engine_thread_data_;

  boost::mt19937 gen_;
  std::unique_ptr<boost::uniform_int<> > one_K_dist_;
  typedef boost::variate_generator<boost::mt19937&, boost::uniform_int<> >
    rng_t;
  std::unique_ptr<rng_t> one_K_rng_;
};


}   // namespace lda
