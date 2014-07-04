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
#include <gflags/gflags.h>
#include <glog/logging.h>
#include "./include/common.hpp"

#include "pi/sysparam.hpp"

using namespace std;

DEFINE_string(nodefile, "", "Node Conf file");
DEFINE_string(linkfile, "", "Link Conf file");
DEFINE_bool(scheduling, true, "flag to disable/enable distributed scheduling");
DEFINE_int32(schedulers, 0, "the number of scheduler machines");
DEFINE_int32(sched_thrds, 0, "threads per scheduler machine");
DEFINE_int64(modelsize, 0, "model parameters to schedule");

DEFINE_int32(worker_thrds, 0, "threads per worker machine");
DEFINE_int32(coordinator_thrds, 0, "helper threads per coordinator machine");

DEFINE_double(bw, 0.0, " base weight to all tasks assigned uniformly");
DEFINE_uint64(maxset, 0, " max size of a round selected by a scheduler thread");
DEFINE_double(infthreshold, INF_THRESHOLD_DEFAULT, " threshold on the degree of interference between a pair of tasks");
DEFINE_int64(pipelinedepth, 0, " pipe line depth = degree of staleness");
DEFINE_bool(apply_permute_col, false, "whether to apply permutation to the col (model) or not ");
// set it to nz in order to prevent starvation of zeroed tasks 

DEFINE_int64(iter, 1000, "how many interations you are going to run");
DEFINE_int64(logfreq, 10, "how frequently measure progress and write log");
DEFINE_string(logdir, "/users/jinkyuk/tmplog/", "log file directory");
DEFINE_string(tlogprefix, "/users/jinkyuk/tmplog/tlogprefix_", "log file directory");

DEFINE_bool(singlemach, false, "specify whether to run on single machine");

void PrintAllFlags(void) {
  std::vector<gflags::CommandLineFlagInfo> flags;
  gflags::GetAllFlags(&flags);
  for (size_t i = 0; i < flags.size(); i++) {
    gflags::CommandLineFlagInfo& flag = flags[i];
    LOG(INFO) << flag.name << ": " << flag.current_value;
  }
}

parameters *pi_initparams(int argc, char **argv, int mpi_rank){

  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  FLAGS_logtostderr = 1;  // enabling to screen log info message out 

  sysparam *sp = new sysparam(FLAGS_nodefile, FLAGS_linkfile, FLAGS_scheduling, 
			      FLAGS_schedulers, FLAGS_sched_thrds, FLAGS_modelsize, FLAGS_worker_thrds, 
			      FLAGS_coordinator_thrds, FLAGS_bw, FLAGS_maxset, FLAGS_infthreshold, FLAGS_pipelinedepth, 
			      FLAGS_apply_permute_col, FLAGS_iter, FLAGS_logfreq, FLAGS_logdir, FLAGS_tlogprefix, FLAGS_singlemach);

  user_initparams(sp); // user function should register data related parameters to the STRADS
  parameters *ret = new parameters(sp);
  if(mpi_rank == 0){
    ret->m_sp->print();  // sysparam.hpp
  }
  return ret;
}
