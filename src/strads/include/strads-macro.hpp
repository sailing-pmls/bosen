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
#pragma once 

#define CONF_FILE_DELIMITER "\n \t"

#define RDMA_S_BPORT (40024)
//#define COM_RDMA_PRIMITIVES

#define RDMA_CPU_AFFINITY (1)
#define RDMA_TEST_MAX_SEND (100000000)

//#define RDMA_USER_BUFFER_POOL (2000)
//#define RDMA_PENDING_OUTQ_SIZE (900)

// for 8
#define RDMA_USER_BUFFER_POOL (32)
// 16
#define RDMA_PENDING_OUTQ_SIZE (12)
// 6

//#define RDMA_USER_BUFFER_POOL (64)
//#define RDMA_PENDING_OUTQ_SIZE (24)

#define MAX_SCHEDULER_THREAD (256) 
// max scheduler threads per a scheduler machine

#define MAX_FN (256) 
// max input file name length for STRADS.
#define MAX_ALIAS (64) 
// max alias name length for user input data

#define MAX_MACHINES (32)

#define SCHEDULING_RANDOM_SEED (0x12000)

#define INF_THRESHOLD_DEFAULT (0.1)

#define INVALID_DOUBLE (-100000000)

#define INVALID_LOGID (-10001)

#define MAX_EVENT_PER_CMD (200)

#define MAX_MCHORD (128)

#define MAX_SET_SIZE (1024*10)

#define STRADS_ZMQ_IO_THREADS 2
// per context. Since just one context is created per machine, this number specifies the number of ZMQ io threads per machines 
// regardless of the number of ZMQ ports 

//#define NO_WEIGHT_SAMPLING


