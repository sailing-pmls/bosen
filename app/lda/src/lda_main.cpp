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
// Date: 2014.03.25

#include "lda_engine.hpp"
#include "context.hpp"
#include <petuum_ps_common/include/petuum_ps.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <thread>
#include <vector>

// Petuum Parameters
//DEFINE_string(hostfile, "", "Path to file containing server ip:port.");
//DEFINE_int32(num_clients, 1, "Total number of clients");
DEFINE_int32(num_worker_threads, 1, "Number of app threads in this client");
//DEFINE_int32(client_id, 0, "Client ID");
DEFINE_int32(word_topic_table_process_cache_capacity, -1,
    "Word topic table process cache capacity. -1 uses FLAGS_num_vocabs.");
DEFINE_int32(ps_row_in_memory_limit, 50000, "Single-machine version only: "
    "max # rows word-topic table that can be held in memory");

// LDA Parameters
DEFINE_string(doc_file, "",
    "File containing document in LibSVM format. Each document is a line.");
DEFINE_double(alpha, 1, "Dirichlet prior on document-topic vectors.");
DEFINE_double(beta, 0.1, "Dirichlet prior on vocab-topic vectors.");
DEFINE_int32(num_topics, 100, "Number of topics.");
DEFINE_int32(num_work_units, 1, "Number of work units. A work unit can be less "
    "or more than one epoch (sweep over dataset), determined by "
    "FLAGS_num_iters_per_work_unit");
DEFINE_int32(compute_ll_interval, 1,
    "Copmute log likelihood over local dataset on every compute_ll_interval "
    "work units");

// Misc
DEFINE_int32(word_topic_table_staleness, 0, "staleness for word-topic table.");
DEFINE_int32(summary_table_staleness, 0,
    "staleness for table containing summary row.");
DEFINE_string(output_file_prefix, "", "LDA results.");
DEFINE_bool(disk_stream, false,
    "Turn it on when memory isn't enough for the dataset.");
DEFINE_int32(disk_stream_batch_size, 1000,
    "Each worker thread gets disk_stream_batch_size docs at a time.");
DEFINE_bool(output_topic_word, false, "Output phi to output_file_prefix.phi. "
    "Each row is a topic.");
DEFINE_bool(output_doc_topic, false, "Output theta to "
    "output_file_prefix.theta.X, X is the client ID");

// No need to change the following.
DEFINE_int32(word_topic_table_id, 1, "ID within Petuum-PS");
DEFINE_int32(summary_table_id, 2, "ID within Petuum-PS");
DEFINE_int32(llh_table_id, 3, "ID within Petuum-PS");
DEFINE_bool(aggressive_cpu, false, "Aggressive usage of CPU");
DEFINE_int32(num_vocabs, -1, "Number of vocabs. This is read from meta-data file.");
DEFINE_int32(max_vocab_id, -1, "Maximum word index, which could be different "
    "from num_vocabs if there are unused vocab indices. "
    "This is read from meta-data file.");

//DEFINE_string(stats_path, "", "Statistics output file");

//DEFINE_string(consistency_model, "SSPPush", "SSP or SSPPush or ...");

//DEFINE_int32(num_bg_threads, 1, "number of background threads");
DEFINE_int32(num_server_threads, 1, "number of server threads");

DEFINE_int32(num_iters_per_work_unit, 1, "number of iterations per work unit");
DEFINE_int32(num_clocks_per_work_unit, 1, "number of clocks per work unit");

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  lda::LDAEngine ldaengine;
  ldaengine.Run(FLAGS_num_worker_threads);
  return 0;
}
