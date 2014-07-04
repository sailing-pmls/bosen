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

/********************************************************
   @author: Jin Kyu Kim (jinkyuk@cs.cmu.edu)
   Distributed scheduler framework 
********************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <sys/time.h>
#include <string>
#include <iostream>
#include <pthread.h>
#include <mpi.h>
#include <assert.h>
#include <zmq.hpp>
#include <dlfcn.h>
#include "./include/common.hpp"
#include "ds/dshard.hpp"
#include "com/comm.hpp"
#include "com/zmq/zmq-common.hpp"
#include "ds/binaryio.hpp"
#include <glog/logging.h>

using namespace std;

int main(int argc, char **argv){

  int mpi_rank, mpi_size;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  assert(mpi_size < MAX_MACHINES);

  sharedctx *pshctx = new sharedctx(mpi_rank);  
  parameters *params = pi_initparams(argc, argv, mpi_rank); // store system parameters and user parameters

  if(mpi_rank == 0)
    params->print_allparams();

  parse_nodefile(params->m_sp->m_nodefile, *pshctx);
  if((unsigned)mpi_size != pshctx->nodes.size()){
    LOG(FATAL) << "Fatal: mpi_size(" << mpi_size <<") != (" << pshctx->nodes.size() << ")" << endl;
  }

  string validip;
  util_find_validip(validip, *pshctx);
  LOG(INFO) << "Rank (" << mpi_rank << ")'s IP found in node file " << validip << endl;

  parse_starlinkfile(params->m_sp->m_linkfile, *pshctx);
  pshctx->bind_params(params); // associate parameters with shared context 
  pshctx->prepare_machine(mpi_size);

  int io_threads= STRADS_ZMQ_IO_THREADS;
  zmq::context_t contextzmq(io_threads);

  if(params->m_sp->m_singlemach){
    assert(mpi_size == 3);
    strads_msg(OUT, "@@@@@ Single Machine Mode \n");
    create_star_ethernet_singlemach(pshctx, contextzmq, mpi_size);
  }else{  
    strads_msg(OUT, "@@@@@ Normal %d machine running \n", mpi_size);
    create_star_ethernet(pshctx, contextzmq, mpi_size, pshctx->nodes[mpi_size-1]->ip);
  }

  pshctx->start_framework();

  return 0;
}
