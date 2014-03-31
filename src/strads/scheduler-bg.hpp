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


#if !defined(SCHEDULER_BG_HPP)
#define SCHEDULER_BG_HPP

#include "strads-queue.hpp"
#include "threadctx.hpp"

void *scheduler_master_background(void *arg);
void *scheduler_client_machine(void *arg);

void scheduler_terminate_to_master_bgthread(int schedlid, tqhandler *tqh);

tqhandler* init_bg_schedthread(int threads, threadctx *tctx);
void reset_channel(threadctx *tctx);

void *scheduler_gate_thread(void *arg);
/* end of application specific data structure declaration */

void _get_thread_col_range(long unsigned int sghcnt, long unsigned int startc, long unsigned int endc, long unsigned int *thrd_startc, long unsigned int *thrd_endc, long unsigned int mythrank);
//void _get_my_column_range(threadctx *tctx, uint64_t partcnt, uint64_t features, uint64_t *startc, uint64_t *endc);

void sched_get_my_column_range(uint64_t myrank, clusterctx *cctx, uint64_t partcnt, uint64_t features, uint64_t *startc, uint64_t *endc);


#endif
