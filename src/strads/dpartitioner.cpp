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
/*********************************************************

  @author: Jin Kyu Kim (jinkyuk@cs.cmu.edu)
  Parallel Lasso problem solver based coordinate descent.

********************************************************/


//iohandler_spmat_mmio_partial_read
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <stdint.h>
#include <sys/time.h>
#include <queue>
#include <assert.h>

#include "dpartitioner.hpp"
#include "threadctx.hpp"
#include "utility.hpp"
#include "iohandler.hpp"
#include "syscom.hpp"
#include "spmat.hpp"
#include "binaryio.hpp"

using namespace std;

std::map<uint64_t, dpartitionctx *> readydpart;

void load_dpartition(dpartitionctx *dpart, threadctx *tctx);
void syncreceive_dpartition(dpartitionctx *dpart, threadctx *tctx);


void send_dpartition_cmd_to_singlem(threadctx *tctx, char *infn, uint64_t v_s, uint64_t v_len, uint64_t h_s, uint64_t h_len, dtype pformat, densitytype sptype, int dst, uint64_t samples, uint64_t features){
 
  dpartitionctx *dpart = (dpartitionctx *)calloc(1, sizeof(dpartitionctx));
  int fnsize = strlen(infn);

  if(fnsize > MAX_FILE_NAME ){
    strads_msg(ERR, "too long file name found in send_dpartition_cmd_to_singlem function \n");
    exit(0);
  }
  strcpy(dpart->fn, infn);

  dpart->sptype = sptype;
  dpart->dparttype = pformat;

  dpart->range.v_s = v_s;
  dpart->range.v_len = v_len;
  dpart->range.v_end = v_s + v_len - 1;

  dpart->range.h_s = h_s;
  dpart->range.h_len = h_len;
  dpart->range.h_end = h_s + h_len - 1;

  dpart->fullsamples = samples;
  dpart->fullfeatures = features;

  syscom_send_amo_comhandler(tctx->comh[WGR] , tctx, (uint8_t *)dpart, sizeof(dpartitionctx), load_dpart, dst, 0); 

  free(dpart);
}


// Send a partitioning command to worker machine from App scheduler 
void send_ooc_dpartition_ooc_cmd_to_singlem(threadctx *tctx, char *infn, uint64_t v_s, uint64_t v_len, uint64_t h_s, uint64_t h_len, dtype pformat, densitytype sptype, int dst, uint64_t samples, uint64_t features, long ooc_hmodflag, long h_modulo, long oocdpartitions, ptype cmdtype){
 
  dpartitionctx *dpart = (dpartitionctx *)calloc(1, sizeof(dpartitionctx));
  int fnsize = strlen(infn);

  if(fnsize > MAX_FILE_NAME ){
    strads_msg(ERR, "too long file name found in send_dpartition_cmd_to_singlem function \n");
    exit(0);
  }
  strcpy(dpart->fn, infn);

  dpart->sptype = sptype;
  dpart->dparttype = pformat;

  dpart->range.v_s = v_s;
  dpart->range.v_len = v_len;
  dpart->range.v_end = v_s + v_len - 1;

  dpart->range.h_s = h_s;
  dpart->range.h_len = h_len;
  dpart->range.h_end = h_s + h_len - 1;

  dpart->fullsamples = samples;
  dpart->fullfeatures = features;

  dpart->range.ooc_hmodflag = ooc_hmodflag;
  dpart->range.h_modulo = h_modulo;
  dpart->range.oocdpartitions = oocdpartitions;

  //  syscom_send_amo_comhandler(tctx->comh[WGR] , tctx, (uint8_t *)dpart, sizeof(dpartitionctx), oocload_dpart, dst, 0); 
  syscom_send_amo_comhandler(tctx->comh[WGR] , tctx, (uint8_t *)dpart, sizeof(dpartitionctx), cmdtype, dst, 0); 

  free(dpart);
}


void send_dpartition_ooc_cmd_to_singlem(threadctx *tctx, char *infn, uint64_t v_s, uint64_t v_len, uint64_t h_s, uint64_t h_len, dtype pformat, densitytype sptype, int dst, uint64_t samples, uint64_t features){
 
  dpartitionctx *dpart = (dpartitionctx *)calloc(1, sizeof(dpartitionctx));
  int fnsize = strlen(infn);

  if(fnsize > MAX_FILE_NAME ){
    strads_msg(ERR, "too long file name found in send_dpartition_cmd_to_singlem function \n");
    exit(0);
  }
  strcpy(dpart->fn, infn);

  dpart->sptype = sptype;
  dpart->dparttype = pformat;

  dpart->range.v_s = v_s;
  dpart->range.v_len = v_len;
  dpart->range.v_end = v_s + v_len - 1;

  dpart->range.h_s = h_s;
  dpart->range.h_len = h_len;
  dpart->range.h_end = h_s + h_len - 1;

  dpart->fullsamples = samples;
  dpart->fullfeatures = features;

  syscom_send_amo_comhandler(tctx->comh[WGR] , tctx, (uint8_t *)dpart, sizeof(dpartitionctx), oocload_dpart, dst, 0); 

  free(dpart);
}


void send_ooc_dpartition_ooc_cmd_to_schedm(threadctx *tctx, char *infn, uint64_t v_s, uint64_t v_len, uint64_t h_s, uint64_t h_len, dtype pformat, densitytype sptype, int dst, uint64_t samples, uint64_t features, long ooc_hmodflag, long h_modulo, long oocdpartitions, ptype cmdtype){
 
  dpartitionctx *dpart = (dpartitionctx *)calloc(1, sizeof(dpartitionctx));
  int fnsize = strlen(infn);

  if(fnsize > MAX_FILE_NAME ){
    strads_msg(ERR, "too long file name found in send_dpartition_cmd_to_singlem function \n");
    exit(0);
  }
  strcpy(dpart->fn, infn);

  dpart->sptype = sptype;
  dpart->dparttype = pformat;

  dpart->range.v_s = v_s;
  dpart->range.v_len = v_len;
  dpart->range.v_end = v_s + v_len - 1;

  dpart->range.h_s = h_s;
  dpart->range.h_len = h_len;
  dpart->range.h_end = h_s + h_len - 1;

  dpart->fullsamples = samples;
  dpart->fullfeatures = features;

  dpart->range.ooc_hmodflag = ooc_hmodflag;
  dpart->range.h_modulo = h_modulo;
  dpart->range.oocdpartitions = oocdpartitions;

  syscom_send_amo_comhandler(tctx->comh[SGR] , tctx, (uint8_t *)dpart, sizeof(dpartitionctx), cmdtype, dst, 0); 

  free(dpart);
}



void send_dpartition_cmd_to_schedm(threadctx *tctx, char *infn, uint64_t v_s, uint64_t v_len, uint64_t h_s, uint64_t h_len, dtype pformat, densitytype sptype, int dst, uint64_t samples, uint64_t features){
 
  dpartitionctx *dpart = (dpartitionctx *)calloc(1, sizeof(dpartitionctx));
  int fnsize = strlen(infn);

  if(fnsize > MAX_FILE_NAME ){
    strads_msg(ERR, "too long file name found in send_dpartition_cmd_to_singlem function \n");
    exit(0);
  }
  strcpy(dpart->fn, infn);

  dpart->sptype = sptype;
  dpart->dparttype = pformat;

  dpart->range.v_s = v_s;
  dpart->range.v_len = v_len;
  dpart->range.v_end = v_s + v_len - 1;

  dpart->range.h_s = h_s;
  dpart->range.h_len = h_len;
  dpart->range.h_end = h_s + h_len - 1;

  dpart->fullsamples = samples;
  dpart->fullfeatures = features;

  syscom_send_amo_comhandler(tctx->comh[SGR] , tctx, (uint8_t *)dpart, sizeof(dpartitionctx), load_dpart, dst, 0); 

  free(dpart);
}


dpartitionctx *dpart_alloc_ctx_only(dpartitionctx *payload, int rank) {


  strads_msg(INF, "dpart_alloc_ctxOnly @@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
  //dpartitionctx *dp =  (dpartitionctx *)calloc(1, sizeof(dpartitionctx));
  uint64_t maxrow, maxcol;
  maxrow = payload->range.v_s + payload->range.v_len; // y
  maxcol = payload->range.h_s + payload->range.h_len; // x

  strads_msg(INF, "@@@@@ dpartition ctx call: Maxrow(%ld) Maxcol(%ld)\n", 
	     maxrow, maxcol);
  dpartitionctx *dp =  new dpartitionctx(maxrow,maxcol);

  strads_msg(INF, "@@@@@ sprow_mat.row_size():%ld sprow_mat.col_size():%ld \n", 
	     dp->sprow_mat.row_size(),
	     dp->sprow_mat.col_size());
  assert(dp);

  dp->dpartid = payload->dpartid;

  dp->dparttype = payload->dparttype;
  dp->sptype    = payload->sptype;

  dp->location = payload->location;

  dp->range.v_s = payload->range.v_s;
  dp->range.h_s = payload->range.h_s;

  dp->range.v_len = payload->range.v_len;
  dp->range.h_len = payload->range.h_len;

  dp->range.h_end = dp->range.h_s + dp->range.h_len - 1;
  dp->range.v_end = dp->range.v_s + dp->range.v_len - 1;

  strcpy(dp->fn, payload->fn);     

  dp->fullsamples = payload->fullsamples;
  dp->fullfeatures = payload->fullfeatures;

  dp->range.oocdpartitions = payload->range.oocdpartitions;
  dp->range.ooc_hmodflag = payload->range.ooc_hmodflag;
  dp->range.h_modulo = payload->range.h_modulo;

  strads_msg(INF, "End dpart_alloc_ctxOnly @@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
  return dp;
}

int dpart_dataload_memalloc_for_ctx(dpartitionctx *dpart, int rank){

  if(dpart->sptype == dense_type){
    if(dpart->dparttype == rowmajor_matrix){    
      // dense + row major pyhsical format      
    }else{
      // dense + column major physical format
    }
  }else{

#if !defined(BINARY_IO_FORMAT)
    if(dpart->dparttype == rowmajor_matrix){
      // dense + row major pyhsical format
      strads_msg(INF, "allocate a row major sparse matrix with %ld by %ld submatrix from file (%s) at rank(%d)\n", 
		 dpart->range.v_end + 1, dpart->range.h_end + 1, dpart->fn, rank);
      iohandler_spmat_mmio_partial_read(dpart->fn, dpart, true, rank);
    }else{
      strads_msg(INF, "allocate a col major sparse matrix with %ld by %ld submatrix from file (%s) at rank (%d)\n", 
		 dpart->range.v_end + 1, dpart->range.h_end + 1, dpart->fn, rank);
      iohandler_spmat_mmio_partial_read(dpart->fn, dpart, true, rank);
    }
#else
    if(dpart->dparttype == rowmajor_matrix){
      // dense + row major pyhsical format
      strads_msg(INF, "allocate a row major sparse matrix with %ld by %ld submatrix from file (%s) at rank(%d)\n", 
		 dpart->range.v_end + 1, dpart->range.h_end + 1, dpart->fn, rank);
      iohandler_spmat_pbfio_partial_read(dpart->fn, dpart, false, rank);
    }else{
      strads_msg(INF, "allocate a col major sparse matrix with %ld by %ld submatrix from file (%s) at rank (%d)\n", 
		 dpart->range.v_end + 1, dpart->range.h_end + 1, dpart->fn, rank);
      iohandler_spmat_pbfio_partial_read(dpart->fn, dpart, false, rank);
    }
#endif
  }
  dpart->readyflag = true;
  return 0;
}


int dpart_dataload_memalloc_for_ctx_wt_ooc(dpartitionctx *dpart, int rank, param_range *selected_prange, int selected_prcnt){

  assert(selected_prange);
  if(dpart->sptype == dense_type){
    if(dpart->dparttype == rowmajor_matrix){    
      // dense + row major pyhsical format      
    }else{
      // dense + column major physical format
    }
  }else{

#if !defined(BINARY_IO_FORMAT)
    strads_msg(ERR, "Fatal, currently, ooc does not support non binary format. It slows down performance critically\n");
    assert(0);
    exit(0);
    if(dpart->dparttype == rowmajor_matrix){
      // dense + row major pyhsical format
      strads_msg(INF, "allocate a row major sparse matrix with %ld by %ld submatrix from file (%s) at rank(%d)\n", 
		 dpart->range.v_end + 1, dpart->range.h_end + 1, dpart->fn, rank);
      iohandler_spmat_mmio_partial_read(dpart->fn, dpart, true, rank);
    }else{
      strads_msg(INF, "allocate a col major sparse matrix with %ld by %ld submatrix from file (%s) at rank (%d)\n", 
		 dpart->range.v_end + 1, dpart->range.h_end + 1, dpart->fn, rank);
      iohandler_spmat_mmio_partial_read(dpart->fn, dpart, true, rank);
    }
#else
    if(dpart->dparttype == rowmajor_matrix){
      // dense + row major pyhsical format
      strads_msg(INF, "allocate a row major sparse matrix with %ld by %ld submatrix from file (%s) at rank(%d)\n", 
		 dpart->range.v_end + 1, dpart->range.h_end + 1, dpart->fn, rank);
      iohandler_spmat_pbfio_partial_read_wt_ooc(dpart->fn, dpart, false, rank, selected_prange, selected_prcnt);
    }else{
      strads_msg(INF, "allocate a col major sparse matrix with %ld by %ld submatrix from file (%s) at rank (%d)\n", 
		 dpart->range.v_end + 1, dpart->range.h_end + 1, dpart->fn, rank);
      iohandler_spmat_pbfio_partial_read_wt_ooc(dpart->fn, dpart, false, rank, selected_prange, selected_prcnt);
    }
#endif

  }
  dpart->readyflag = true;
  return 0;
}



dpartitionctx *_alloc_dpart(ptype type, void *ptr){

  dpartitionctx *dp =  (dpartitionctx *)calloc(1, sizeof(dpartitionctx));
  assert(dp);

  switch(type){
  case load_dpart:
    {
      load_dpart_msg *payload = (load_dpart_msg *)ptr;      
      dp->dpartid = payload->dpartid;
      dp->dparttype = payload->dparttype;
      dp->location = payload->location;
      dp->range.v_s = payload->v_s;
      dp->range.h_s = payload->h_s;

      dp->range.v_len = payload->v_len;
      dp->range.h_len = payload->h_len;

      dp->range.h_end = dp->range.h_s + dp->range.h_len - 1;
      dp->range.v_end = dp->range.v_s + dp->range.v_len - 1;
      strcpy(dp->fn, payload->fn);     
    }
    break;

  case receive_dpart:
    {
      receive_dpart_msg *payload = (receive_dpart_msg *)ptr;      
      dp->dpartid = payload->dpartid;
      dp->dparttype = payload->dparttype;
      dp->location = payload->location;

      dp->range.v_s = payload->v_s;
      dp->range.h_s = payload->h_s;

      dp->range.v_len = payload->v_len;
      dp->range.h_len = payload->h_len;

      dp->range.v_end = dp->range.v_s + dp->range.v_len - 1;
      dp->range.h_end = dp->range.h_s + dp->range.h_len - 1;
      dp->srcrank = payload->srcrank;
    }
    break;

  default:

    break;
  }

  return dp;
}

// TODO: add more functions for various types 
pthread_t *dpartitioner_run(threadctx *ctx, char *msgstring){
  
  int start = sizeof(com_header);
  pthread_t *retval=NULL;  
  com_header *msg = (com_header *)msgstring;

  switch(msg->type){
    
  case load_dpart:
    {
      load_dpart_msg *payload = (load_dpart_msg *)&msg[start];
      //int cstart = start + sizeof(load_dpart_msg);
      //uint32_t crc = *(uint32_t *)&msg[cstart];
      dpartitionctx *entry = _alloc_dpart(load_dpart, (void *)payload);
      load_dpartition(entry, ctx);
      readydpart.insert(std::pair<uint64_t, dpartitionctx *>(entry->dpartid, entry));
    }
    break;
    
  default: 

    break;
  }

  return retval;
}

bool dpartitioner_complete_async(threadctx, dpartitionctx **dparray, int size){




  return false;
}

void check_submatrix_sanity(long row, long col, rangectx *submat){
  assert((submat->v_s+submat->v_len-1) <= (row-1));
  assert((submat->h_s+submat->h_len-1) <= (col-1));
}

void alloc_submatrix(dpartitionctx *dpart, long v_len, long h_len){
  dpart->row_mat = (double **)calloc(h_len, sizeof(double *));
  assert(dpart->row_mat);
  for(long i=0; i<v_len; i++){
    dpart->row_mat[i] = (double *)calloc(h_len, sizeof(double));
    assert(dpart->row_mat[i]);
  }
}


// load a data partition from a remote node 
void load_dpartition(dpartitionctx *dpart, threadctx *tctx){

  char *tmpbuf = (char *)calloc(IOHANDLER_BUFLINE_MAX, sizeof(char));
  assert(tmpbuf);
  long rows=0, cols=0;

  assert(dpart->location == in_localdisk);
  if(dpart->openflag){
    dpart->fd = fopen(dpart->fn, "rt" );
    if(dpart->fd == NULL){
      strads_msg(ERR, "Can not open file (%s) at machine (%d) \n",
		 dpart->fn, tctx->rank);
    }
    dpart->openflag = true; 
  }

  switch(dpart->dparttype){

  case colmajor_matrix:
    strads_msg(ERR, "sub matrix loading vs(%ld) vl(%ld) hs(%ld) hl(%ld)\n", 
	       dpart->range.v_s, dpart->range.v_len, dpart->range.h_s, dpart->range.h_len);

    get_datainfo(dpart->fd, &rows, &cols);
    check_submatrix_sanity(rows, cols, &dpart->range);
    alloc_submatrix(dpart, dpart->range.v_len, dpart->range.h_len);
    iohandler_diskio_loadsubmat(dpart, dpart->range.v_s, dpart->range.v_len, dpart->range.h_s, dpart->range.h_len); 
    break;

  default:
    strads_msg(ERR, "Unknown type of data partition at machine (%d)\n", tctx->rank);
    assert(false);
    break;
  }
}
