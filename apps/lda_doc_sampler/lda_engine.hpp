// Author: Dai Wei (wdai@cs.cmu.edu)
// Date: 2014.03.29

#pragma once

#include "lda_document.hpp"
#include "fast_doc_sampler.hpp"
#include "lda_stats.hpp"
#include "context.hpp"
#include <petuum_ps/include/petuum_ps.hpp>
#include <cstdint>
//#include <boost/thread.hpp>
//#include <boost/thread/tss.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>
#include <utility>
#include <vector>
#include <string>
#include <set>

namespace lda {

// Engine takes care of the entire pipeline of LDA, from reading data to
// spawning threads, to recording execution time and loglikelihood.
class LDAEngine {
public:
  LDAEngine();

  // Read libSVM formatted document (.dat) data. This should only be called
  // once by one thread.  Returning # of vocabs.
  int32_t ReadData(const std::string& doc_file);

  // The main thread (in main()) should spin threads, each running Start()
  // concurrently.
  void Start();

  // The summary row will be refreshed every kInverseContentionProb iteration,
  // causing contention probability to be 1/kInverseContentionProb. It has to
  // be strictly positive.
  static const int32_t kInverseContentionProb;

private:  // private functions
  // Randomly generate a topic on {1,..., K_} and add to doc_word_topics.
  void AddWordTopics(DocumentWordTopicsPB* doc_word_topics, int32_t word,
      int32_t num_tokens);

  // Get a document from docs_.
  inline LDACorpus::iterator GetOneDoc();

private:  // private data
  int32_t K_;   // number of topics

  // Number of application threads.
  int32_t num_threads_;

  // vocabs in this data partition.
  std::set<int32_t> local_vocabs_;

  // Compute complete log-likelihood (ll) every compute_ll_interval_
  // iterations.
  int32_t compute_ll_interval_;

  // Local barrier across threads.
  std::unique_ptr<boost::barrier> process_barrier_;

  LDACorpus docs_;
  LDACorpus::iterator docs_iter_;
  std::mutex docs_mtx_;

  std::atomic<int32_t> thread_counter_;

  // # of tokens processed in a Clock() call.
  std::atomic<int32_t> num_tokens_clock_;

  // Random number generator stuff.
  boost::mt19937 gen_;
  std::unique_ptr<boost::uniform_int<> > one_K_dist_;
  typedef boost::variate_generator<boost::mt19937&, boost::uniform_int<> >
    rng_t;
  std::unique_ptr<rng_t> one_K_rng_;
};

}   // namespace lda
