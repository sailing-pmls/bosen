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

#include "abstract_doc_loader.hpp"
#include "workload_manager.hpp"
#include "fast_doc_sampler.hpp"
#include "lda_stats.hpp"
#include "context.hpp"
//#include "sorted_vector_map_row.hpp"
#include <petuum_ps_common/include/ps_app.hpp>
#include <petuum_ps_common/include/petuum_ps.hpp>
#include <cstdint>
#include <utility>
#include <vector>
#include <string>
#include <set>

namespace lda {

// Engine takes care of the entire pipeline of LDA, from reading data to
// spawning threads, to recording execution time and loglikelihood.
class LDAEngine : public petuum::PsApp{
public:
  // LDAEngine constructor initialize master data loader and set
  // FLAGS_num_vocabs and FLAGS_max_vocab_id.
  LDAEngine();
  void InitApp() override;

  // The main thread (in main()) should spin threads, each running Start()
  // concurrently.
  // void Start();

  std::vector<petuum::TableConfig> ConfigTables() override;

  void WorkerThread(int client_id, int thread_id) override;

private:  // private functions
  void SaveLLH(int up_to_ith_llh);

  void ComputeLLH(int32_t ith_llh, int32_t iter, int32_t client_id,
      int32_t num_clients, int32_t thread_id, int32_t vocab_id_start,
      int32_t vocab_id_end,
      const petuum::HighResolutionTimer& total_timer,
      WorkloadManager* workload_mgr);

  // Compute normalized topic word distribution (phi).
  std::vector<std::vector<float> > GetTopicWordDist();

  void SaveTopicWordDist();

  void SaveDocTopicDist(WorkloadManager* workload_mgr);

private:  // private data
  int32_t K_;   // number of topics

  // vocabs in this data partition.
  std::set<int32_t> local_vocabs_;
  std::mutex local_vocabs_mtx_;

  // Compute complete log-likelihood (ll) every compute_ll_interval_
  // iterations.
  int32_t compute_ll_interval_;

  int32_t num_threads_;

  // LDAStats is thread-safe, but has to be initialized after PS tables are
  // created (thus can't be initialized in constructor).
  std::unique_ptr<LDAStats> lda_stats_;


  //Corpus corpus_;

  std::unique_ptr<AbstractMasterDocLoader> master_loader_; 


  // # of tokens processed in a work_unit
  //std::atomic<int32_t> num_tokens_this_work_unit_;
  //std::atomic<int32_t> num_docs_this_work_unit_;
};

}   // namespace lda
