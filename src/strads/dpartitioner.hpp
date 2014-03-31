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
  data structures for Petuum distributed memory support 

  The first draft in Oct 2013

********************************************************/

#if !defined(DPARTITIONER_HPP_)
#define DPARTITIONER_HPP_

/* #define _GNU_SOURCE */ 
#include "basectx.hpp"
#include "systemmacro.hpp"
#include "comm_handler.hpp"
#include "spmat.hpp"

//#include "threadctx.hpp"
#include <pthread.h>

#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/matrix_proxy.hpp>
#include <boost/numeric/ublas/matrix_sparse.hpp>
#include <boost/numeric/ublas/io.hpp>

//using namespace boost::numeric::ublas;  

enum dpstate { in_localmemory, in_localdisk, in_rmemory, in_rdisk };
enum dtype { colmajor_matrix, rowmajor_matrix, graph_vtx, graph_edge };
enum densitytype { dense_type, sparse_type };
 
typedef struct{
  long v_s;  // vertical pos - row matrix 
  long v_end;
  long v_len; 

  long h_s;  // horizontal pos - column matrix 
  long h_end;
  long h_len;

  long v_modulo; // partition by column 
  long h_modulo; // partition by sample

  long ooc_vmodflag;
  long ooc_hmodflag;

  long oocdpartitions;

}rangectx;

typedef struct{

  long portlist[MAX_MACHINES];   
  char ipstrlist[MAX_MACHINES][64]; 
  commtest::comm_handler comm[MAX_MACHINES]; 

}srclist;

class dpartitionctx {
public:
  dpartitionctx(long unsigned int n, long unsigned int m) :
    sprow_mat(n, m), spcol_mat(n, m)
  {}

  bool readyflag;
  uint64_t dpartid;  // 0000(machine) 000000(local id) 

  densitytype sptype; // dense or sparse 
  dtype dparttype; // physical format -- row or col major 
  char fn[MAX_FILE_NAME];  // abs path of input file 

  uint64_t fullsamples;
  uint64_t fullfeatures;
  
  rangectx range;
  FILE *fd;

  bool openflag;
  bool avail;
  dpstate location;

  int srcrank; // if the data comes from a remote node 
  pthread_t handler_thid; // if async receive is used

  double **row_mat;
  double **col_mat;

  uint64_t *vtxlist;
  double *edgematrix;
  /* TODO: support sparse matrix 
           support more data type as adding more algorithms */

  row_spmat sprow_mat;
  col_spmat spcol_mat;
  //  class row_spmat sptest;
};



bool dpartitioner_complete_async(dpartitionctx **dp_array, int size); 
dpartitionctx *dpart_alloc_ctx_only(dpartitionctx *payload, int rank);
int dpart_dataload_memalloc_for_ctx(dpartitionctx *dpart, int rank);
int dpart_dataload_memalloc_for_ctx_wt_ooc(dpartitionctx *dpart, int rank, param_range *selected_prange, int selected_prcnt);

#endif
