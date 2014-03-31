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


#if !defined(SYSTEMMACRO_HPP)
#define SYSTEMMACRO_HPP

/* Max limit on the number of vertices in correlation (dependency) graph. */
#define DEPENDENCY_GRAPH_ELEMENT_MAX (1024*1024)

#define IOHANDLER_BUFLINE_MAX (2000000*20)

#define TOLERATE_PRECISION_ERROR (0.00000000000001)

#define MAX_SCHED_MACH (128)

#define MAX_SCHED_THREADS (128)

#define SYSTEMCONFIG "system.conf"

/* max number of vertices and edges handled by one background scheduler thread */
#define MAX_VTX_PER_SUBGRAPH (10000)
//#define MAX_EDGE_PER_SUBGRAPH (MAX_VTX_PER_SUBGRAPH*200)

/* used by f2b message when sending bulk loading cmd */


#define MAX_NZ_DISPATCH  (1024)

#define RHOTHRESHOLD (0.1)

#define NNZ_MAX (1024)
#define NZ_MAX  (2048)

#define MAX_EDGES_IN_MSG (1024)

#define MAX_VTX_IN_MSG (1024)

#define SYSTEM_MACRO_INVALID_INDEX (10000000000000)

#define MAX_PENDING_B2F_MSG (20)
//#define MAX_PENDING_B2F_MSG (20)

/* the maximum number of pending message from a background thread to the user scheduler */
/* about 10 might be a good number */

#define OUTFILENAME_MAX (4096)
#define ZMQ_PORT_NUMBER "8000"

#define ZMQ_SCHED_PORT_NUMBER "8181"

#define MAX_PARAM_CHANGE (256)

#define MAX_MACHINES (1024)

#define MAX_FILE_NAME (128)

#define MAX_SUBSCRIBER (128)

/* base minimun prioiry for sampling */

//#define PRIORITY_MINUNIT (0.000001) // for bio app... 
#define PRIORITY_MINUNIT (1)


#define MAX_SAMPLING_SIZE (100000)

#define PSCHED_INF_THRESHOLD (0.1)
//#define PSCHED_INF_THRESHOLD (0.0001)


#define LIGHT_CARE_CNT (50)
#define MIN_UNIT_CHANGE_SIGNATURE 0xFFFFFFFFFFFFFFFF
#define DESIRED_CHANGE_SIGNATURE 0xEFFFFFFFFFFFFFEE

// when you change the max row, col, 
// please think about memory consumtion of boost sparse mat library. 
// it might assign 8 bytes per each row or col in worst scenario even though 
// you just assign two entries at the beginnign row(col) and last row(col).
#define PETUUM_MAX_ROW 1000000000
#define PETUUM_MAX_COL 1000000000

#define MAX_DELTA_SIZE (10000000)

#endif
