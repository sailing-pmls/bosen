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
#if !defined(DELTA_HPP_)
#define DELTA_HPP_

#include <map>
#include <list>
#include "threadctx.hpp"
#include "strads-queue.hpp"
#include "utility.hpp"
#include "syscom.hpp"

using namespace std;

typedef struct{
  long size;
  double *pool;

  long maxpoolsize;

  double fullrangesize;

  double priority_minunit;

  int partno;
  //dataset_t dmap;           
  //value_index_t& p_delta_sort; /* sorted list of promising deltas */
  //std::unordered_set<std::long> np_delta; /* non promising deltas less than a threshold (ex. 10 to -9) */
  //long npd_size;
}st_priority;

typedef struct{
  long *sample;
  long rsamplesize;
  long maxsamplesize;
  double *rndpoints;
}st_sample;

typedef struct{
  long maxsize;
  long *taskid; // local offset id 
  double *newpriority;
  long changed;
}st_prioritychange;

#define MAX_SAMPLE_SIZE (MAX_SAMPLING_SIZE)
// MAX_SAMPLINGSIZE is defined in systemmacro.hpp file 

typedef struct{
  uint64_t size;
  uint64_t samples[MAX_SAMPLE_SIZE];
}sampleset;

typedef struct{
  uint64_t size;
  double   deltalist[MAX_DELTA_SIZE];
  uint64_t idxlist[MAX_DELTA_SIZE];
}deltaset;


long unsigned int psched_get_samples(void *recvbuf, sampleset *set);
int psched_init_sampling(st_priority *stpriority, long maxpoolsize, st_sample *stsample, long maxsetsize, st_prioritychange *pchange, long maxsize);
int psched_do_sampling(st_sample *stsample, st_priority *stpriority, long desired);
int psched_update_priority(st_priority *stpriority, st_prioritychange *pchange);
int psched_check_interference(st_sample *stsample, st_priority *stpriority, long desired, long result, sghthreadctx *tctx);
//void psched_process_delta_update(deltaset *deltas, threadctx *tctx, double newminunit);
void psched_process_delta_update(deltaset *deltas, threadctx *tctx, double newminunit, uint64_t newdesired);
void psched_start_scheduling(deltaset *deltas, threadctx *tctx, double newminunit);
void psched_send_token(threadctx *tctx, long unsigned int partno);

//void psched_send_blockcmd(threadctx *tctx, uint64_t partno, uint64_t blockcmd);
void psched_send_blockcmd(threadctx *tctx, uint64_t partno, ptype packettype, uint64_t blockcmd);


int psched_light_check_interference(st_sample *stsample, st_priority *stpriority, long desired, long result, sghthreadctx *tctx);


void psched_process_delta_update_onepartition(deltaset *deltas, threadctx *tctx, double newminunit, uint64_t newdesired, uint64_t onepart, uint64_t blockcmd);

void psched_process_delta_update_with_partno(deltaset *deltas, threadctx *tctx, double newminunit, uint64_t newdesired, int partno, uint64_t blockcmd);

// obsolete code 
//int init_sampling(st_sample *stsample, st_priority *stdelta, problemctx *pctx);
//int do_sampling(st_sample *stsample, st_priority *stdelta, problemctx *pctx, int *vlist, long vsize);

#endif
