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
  Parallel Lasso problem solver based coordinate descent.

********************************************************/

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

#include "iohandler.hpp"
#include "systemmacro.hpp"
#include "utility.hpp"
//#include "mm_io.hpp"
#include "dpartitioner.hpp"

using namespace std;


using namespace boost::numeric::ublas;

static void _standardizeInput(double **MX, double *MY, int n, int p);


#if 0 
long iohandler_spmat_mmio_partial_read(char *fn, dpartitionctx *dpart, bool matlabflag, int rank){

  long ret_code;
  MM_typecode matcode;
  FILE *f;
  long unsigned int maxrow, maxcol, nzcnt;   
  long unsigned int tmprow, tmpcol;
  double tmpval;

  strads_msg(ERR, "iohandler_partialread:rank(%d)  fn(%s), v_s(%ld) v_end(%ld) v_len(%ld) h_S(%ld) h_end(%ld) h_len(%ld)\n", 
	     rank, fn, dpart->range.v_s, dpart->range.v_end, dpart->range.v_len,  
	     dpart->range.h_s, dpart->range.h_end, dpart->range.h_len);

  f = fopen(fn, "r");
  assert(f);
  assert(fn);

  if(mm_read_banner(f, &matcode) != 0){
    strads_msg(ERR, "mmio fatal error. Could not process Matrix Market banner.\n");
    exit(1);
  }

  /*  This is how one can screen matrix types if their application */
  /*  only supports a subset of the Matrix Market data types.      */
  if(mm_is_complex(matcode) && mm_is_matrix(matcode) && mm_is_sparse(matcode)){
    strads_msg(ERR, "mmio fatal error. this application does not support ");
    strads_msg(ERR, "\tMarket Market type: [%s]\n", mm_typecode_to_str(matcode));
    exit(1);
  }

  /* find out size of sparse matrix .... */
  if((ret_code = mm_read_mtx_crd_size(f, &maxrow, &maxcol, &nzcnt)) !=0){
    strads_msg(ERR, "mmio fatal error. Sorry, this application does not support");
    exit(1);
  }

  /* NOTE: when reading in doubles, ANSI C requires the use of the "l"  */
  /*   specifier as in "%lg", "%lf", "%le", otherwise errors will occur */
  /*  (ANSI C X3.159-1989, Sec. 4.9.6.2, p. 136 lines 13-15)            */
  strads_msg(ERR, "@@@@@ %ld nzcnt to read\n", nzcnt);
  char *chbuffer = (char *)calloc(1024*1024*2, sizeof(char));
  uint64_t nzprogress=0;

  while(fgets(chbuffer, 1024*1024, f) != NULL){
    sscanf(chbuffer, "%lu %lu %lf", &tmprow, &tmpcol, &tmpval);   
    if(0){
      strads_msg(ERR, "buffer line (%s)\n", chbuffer);
      strads_msg(ERR, " tmprow(%lu)  tmpcol(%lu) tmpval(%lf)\n", 
		 tmprow, tmpcol, tmpval);
    }

    if(matlabflag){
      tmprow--;  /* adjust from 1-based to 0-based */
      tmpcol--;    
    }

    if(!(tmprow < maxrow)){
      strads_msg(ERR, "\n tmprow (%lu) maxrow(%lu)\n", 
		 tmprow, maxrow);
      exit(0);
    }

    if(!(tmpcol < maxcol)){
      strads_msg(ERR, "\n tmpcol (%lu) maxcol(%lu)\n", 
		 tmpcol, maxcol);
      exit(0);
    }

    assert(tmprow < maxrow);
    assert(tmpcol < maxcol);

    if((tmprow >= (long unsigned int)dpart->range.v_s) 
       && (tmprow <= (long unsigned int)dpart->range.v_end)
       && (tmpcol >= (long unsigned int)dpart->range.h_s)
       && (tmpcol <= (long unsigned int)dpart->range.h_end)) {

      if(dpart->dparttype == rowmajor_matrix){

	strads_msg(INF, "Row Major (%ld, %ld) = %lf", tmprow, tmpcol, tmpval);
	dpart->sprow_mat(tmprow, tmpcol) = tmpval;
      }else{ // column major 

	strads_msg(INF, "Col Major (%ld, %ld) = %lf", tmprow, tmpcol, tmpval);
	dpart->spcol_mat(tmprow, tmpcol) = tmpval;
      }

      nzprogress++;
      if(nzprogress %10000 == 0){
	strads_msg(INF, " %ld th nzelement (%s) at rank (%d)", nzprogress, chbuffer, rank);
      }
    }

  }

#if 0 
  // since this function load partial of whole matrix, 
  // total nzcnt might not be equal to the nz in the header 
  if(nzprogress != nzcnt){
    strads_msg(ERR, "Very WEIRD. Some thing wrong with data format header(%ld) read (%ld) \n",
	       nzcnt, nzprogress);	      
    exit(0);
  }
#endif 

  fclose(f);
  return 0;
}


// clear previous stored sparse matrix 
// and load data contents into it
long iohandler_spmat_mmio_read_size(char *fn, uint64_t *sample, uint64_t *feature, uint64_t *nz){

  long ret_code;
  MM_typecode matcode;
  FILE *f;
  long unsigned int maxrow, maxcol, nzcnt;   

  strads_msg(ERR, "iohandler_spmat_mmio_read_size input file name: %s\n", fn);

  f = fopen(fn, "r");
  assert(f);
  assert(fn);

  if(mm_read_banner(f, &matcode) != 0){
    strads_msg(ERR, "mmio fatal error. Could not process Matrix Market banner.\n");
    exit(1);
  }

  /*  This is how one can screen matrix types if their application */
  /*  only supports a subset of the Matrix Market data types.      */
  if(mm_is_complex(matcode) && mm_is_matrix(matcode) && mm_is_sparse(matcode)){
    strads_msg(ERR, "mmio fatal error. this application does not support ");
    strads_msg(ERR, "\tMarket Market type: [%s]\n", mm_typecode_to_str(matcode));
    exit(1);
  }

  /* find out size of sparse matrix .... */
  if((ret_code = mm_read_mtx_crd_size(f, &maxrow, &maxcol, &nzcnt)) !=0){
    strads_msg(ERR, "mmio fatal error. Sorry, this application does not support");
    exit(1);
  }

  *sample = maxrow;
  *feature = maxcol;
  *nz = nzcnt;

  fclose(f);
  return 0;

}


long iohandler_spmat_mmio_read(char *fn, dpartitionctx *dpart, dtype type){

  long ret_code;
  MM_typecode matcode;
  FILE *f;
  long unsigned int maxrow, maxcol, nzcnt;   
  long unsigned int tmprow, tmpcol;
  double tmpval;

  f = fopen(fn, "r");
  assert(f);
  assert(fn);

  if(mm_read_banner(f, &matcode) != 0){
    strads_msg(ERR, "mmio fatal error. Could not process Matrix Market banner.\n");
    exit(1);
  }

  /*  This is how one can screen matrix types if their application */
  /*  only supports a subset of the Matrix Market data types.      */
  if(mm_is_complex(matcode) && mm_is_matrix(matcode) && mm_is_sparse(matcode)){
    strads_msg(ERR, "mmio fatal error. this application does not support ");
    strads_msg(ERR, "\tMarket Market type: [%s]\n", mm_typecode_to_str(matcode));
    exit(1);
  }

  /* find out size of sparse matrix .... */
  if((ret_code = mm_read_mtx_crd_size(f, &maxrow, &maxcol, &nzcnt)) !=0){
    strads_msg(ERR, "mmio fatal error. Sorry, this application does not support");
    exit(1);
  }

  /* NOTE: when reading in doubles, ANSI C requires the use of the "l"  */
  /*   specifier as in "%lg", "%lf", "%le", otherwise errors will occur */
  /*  (ANSI C X3.159-1989, Sec. 4.9.6.2, p. 136 lines 13-15)            */

#if 0 
  if(type == colmajor_matrix){
    dpart->spcol_mat.clear();
  }else{
    assert(dpart->dparttype == rowmajor_matrix); 
  }

  if(type == rowmajor_matrix){
    dpart->sprow_mat.clear();
  }else{
    assert(dpart->dparttype == colmajor_matrix); 
  }
#endif 

  for(uint64_t i=0; i<nzcnt; i++){
    assert(fscanf(f, "%lu %lu %lg\n", &tmprow, &tmpcol, &tmpval) == 3);
    tmprow--;  /* adjust from 1-based to 0-based */
    tmpcol--;

    assert(tmprow < maxrow);
    assert(tmpcol < maxcol);

    if(type == colmajor_matrix){      
      dpart->spcol_mat(tmprow, tmpcol) = tmpval;

    }else{
      dpart->sprow_mat(tmprow, tmpcol) = tmpval;
    }
  }

  fclose(f);
  return 0;
}




// this code captured from matrix market web site example code. 
// and modified for our purpose 
long iohandler_spmat_mmio_write(char *fn, dpartitionctx *dpart){
  MM_typecode matcode;                        
  uint64_t maxrow=0, maxcol=0, nzcnt=0;
  FILE *fp = fopen(fn, "wt");
  assert(fp);

  mm_initialize_typecode(&matcode);
  mm_set_matrix(&matcode);
  mm_set_coordinate(&matcode);
  mm_set_real(&matcode);

  mm_write_banner(fp, matcode); 
  
  if(dpart->dparttype == colmajor_matrix){
    maxrow = dpart->spcol_mat.row_size();
    maxcol = dpart->spcol_mat.col_size();
    for(uint64_t row=0; row < maxrow; row++){
      for(uint64_t col=0; col < maxcol; col++){
	if(dpart->spcol_mat.get(row, col) != 0.0){
	  nzcnt++;
	}
      }
    }
  }

  if(dpart->dparttype == rowmajor_matrix){
    maxrow = dpart->sprow_mat.row_size();
    maxcol = dpart->sprow_mat.col_size();
    for(uint64_t row=0; row < maxrow; row++){
      for(uint64_t col=0; col < maxcol; col++){
	if(dpart->sprow_mat.get(row, col) != 0.0){
	  nzcnt++;
	}
      }
    }
  }

  mm_write_mtx_crd_size(fp, maxrow, maxcol, nzcnt);
  /* NOTE: matrix market files use 1-based indices, i.e. first element
     of a vector has index 1, not 0.  */
  uint64_t nzidx=0;
  double tmpval;

  if(dpart->dparttype == colmajor_matrix){
    for(uint64_t row=0; row < maxrow; row++){
      for(uint64_t col=0; col < maxcol; col++){
	if(dpart->spcol_mat.get(row, col) != 0.0){
	  nzidx++;	
	  tmpval = dpart->spcol_mat.get(row, col);
	  fprintf(fp, "%lu %lu %20.14lg\n", row+1, col+1, tmpval);
	}
      }
    }
  }else{

    assert(dpart->dparttype == rowmajor_matrix);
    for(uint64_t row=0; row < maxrow; row++){
      for(uint64_t col=0; col < maxcol; col++){
	if(dpart->sprow_mat.get(row, col) != 0.0){
	  nzidx++;	
	  tmpval = dpart->sprow_mat.get(row, col);
	  fprintf(fp, "%lu %lu %20.14lg\n", row+1, col+1, tmpval);
	}
      }
    }
  }

  assert(nzidx == nzcnt);
  fclose(fp);
  return 0;
}

// this code captured from matrix market web site example code. 
// and modified for our purpose 
int iohandler_mmio_write(char *fn){
  long unsigned int nz=4;
  long unsigned int M=10;
  long unsigned int N=10;
  
  MM_typecode matcode;                        
  long unsigned int I[4] = { 0, 4, 2, 8 };
  long unsigned int J[4] = { 3, 8, 7, 5 };
  double val[4] = {1.1, 2.2, 3.2, 4.4};
  long unsigned int i;
  //  char *fn = (char *)calloc(512, sizeof(char));
  //sprintf(fn, "10r10col.crg.txt");
  FILE *fp = fopen(fn, "wt");
  assert(fp);

  mm_initialize_typecode(&matcode);
  mm_set_matrix(&matcode);
  mm_set_coordinate(&matcode);
  mm_set_real(&matcode);

  mm_write_banner(fp, matcode); 
  mm_write_mtx_crd_size(fp, M, N, nz);

  /* NOTE: matrix market files use 1-based indices, i.e. first element
     of a vector has index 1, not 0.  */
  for (i=0; i<nz; i++)
    fprintf(fp, "%lu %lu %20.14lg\n", I[i]+1, J[i]+1, val[i]);

  return 0;
}


// this function captured from matrix market web site example code 
// and modified for our purpose 
long iohandler_memalloc_mmio_read(char *fn)
{
  long ret_code;
  MM_typecode matcode;
  FILE *f;
  long unsigned int M, N, nz;   
  long unsigned int i, *I, *J;
  double *val;

  if(!fn){
      fprintf(stderr, "Usage: %s [martix-market-filename]\n", fn);
      exit(1);
  }
  else{ 
    if ((f = fopen(fn, "r")) == NULL) 
      exit(1);
  }

  if(mm_read_banner(f, &matcode) != 0){
    printf("Could not process Matrix Market banner.\n");
    exit(1);
  }

  /*  This is how one can screen matrix types if their application */
  /*  only supports a subset of the Matrix Market data types.      */
  if(mm_is_complex(matcode) && mm_is_matrix(matcode) && 
      mm_is_sparse(matcode)){
    printf("Sorry, this application does not support ");
    printf("Market Market type: [%s]\n", mm_typecode_to_str(matcode));
    exit(1);
  }

  /* find out size of sparse matrix .... */

  if((ret_code = mm_read_mtx_crd_size(f, &M, &N, &nz)) !=0)
    exit(1);

  /* reseve memory for matrices */
  I = (long unsigned int *) malloc(nz * sizeof(long unsigned int));
  J = (long unsigned int *) malloc(nz * sizeof(long unsigned int));
  val = (double *) malloc(nz * sizeof(double));

  /* NOTE: when reading in doubles, ANSI C requires the use of the "l"  */
  /*   specifier as in "%lg", "%lf", "%le", otherwise errors will occur */
  /*  (ANSI C X3.159-1989, Sec. 4.9.6.2, p. 136 lines 13-15)            */

  for(i=0; i<nz; i++){
    assert(fscanf(f, "%lu %lu %lg\n", &I[i], &J[i], &val[i]) == 3);
    I[i]--;  /* adjust from 1-based to 0-based */
    J[i]--;
  }

  if(f !=stdin) 
    fclose(f);
  /************************/
  /* now write out matrix  for TEST only*/
  /************************/
  mm_write_banner(stdout, matcode);
  mm_write_mtx_crd_size(stdout, M, N, nz);
  for(i=0; i<nz; i++)
    fprintf(stdout, "%lu %lu %20.19g\n", I[i]+1, J[i]+1, val[i]);
  return 0;
}

#endif 


void iohandler_sf_count(char *fn, long *samples, long *features){
  FILE *fd;
  char *ptr;
  long fcnt=0, scnt=0;

  char *tmpbuf = (char *)calloc(IOHANDLER_BUFLINE_MAX, sizeof(char));
  
  fd = fopen(fn, "r");
  if(fd == NULL){
    strads_msg(ERR, "fatal: feature count error faild to open %s file \n", fn);
    exit(0);
  }
  
  if(fgets(tmpbuf, IOHANDLER_BUFLINE_MAX, fd)){
    scnt++;
  }

  ptr = strtok(tmpbuf, INPUTDELIM);
  while(ptr != NULL){
    fcnt++;
    ptr = strtok(NULL, INPUTDELIM);
  }
  while(fgets(tmpbuf, IOHANDLER_BUFLINE_MAX, fd)){
    scnt++;
  }
  fclose(fd);

  *samples = scnt;
  *features= fcnt;

  strads_msg(INF, " %ld samples found\n", *samples);
  strads_msg(INF, " %ld features found\n", *features);
}


// remove this code .... later
void iohandler_sf_count_tmp(char *fn, uint64_t *samples, uint64_t *features){
  FILE *fd;
  char *ptr;
  long fcnt=0, scnt=0;

  char *tmpbuf = (char *)calloc(IOHANDLER_BUFLINE_MAX, sizeof(char));
  
  fd = fopen(fn, "r");
  if(fd == NULL){
    strads_msg(ERR, "fatal: feature count error faild to open %s file \n", fn);
    exit(0);
  }
  
  if(fgets(tmpbuf, IOHANDLER_BUFLINE_MAX, fd)){
    scnt++;
  }

  ptr = strtok(tmpbuf, INPUTDELIM);
  while(ptr != NULL){
    fcnt++;
    ptr = strtok(NULL, INPUTDELIM);
  }
  while(fgets(tmpbuf, IOHANDLER_BUFLINE_MAX, fd)){
    scnt++;
  }
  fclose(fd);

  *samples = scnt;
  *features= fcnt;

  strads_msg(INF, " %ld samples found\n", *samples);
  strads_msg(INF, " %ld features found\n", *features);
}


void iohandler_allocmem_problemctx(problemctx *ctx, long samples, long features, long cmflag){

  assert(ctx->X == NULL);
  assert(ctx->cmX == NULL);
  assert(ctx->Y == NULL);
  assert(ctx->pxsum == NULL);
  assert(ctx->pxsqsum == NULL);
  assert(ctx->cmflag == cmflag);

  ctx->X = (double **)calloc(samples, sizeof(double *));
  for(long i=0; i < samples; i++){
    ctx->X[i] = (double *)calloc(features, sizeof(double));
  }

  ctx->Y = (double *)calloc(samples, sizeof(double));

  ctx->pxsum = (double *)calloc(features, sizeof(double));
  ctx->pxsqsum = (double *)calloc(features, sizeof(double));

  if(cmflag == 1){
    ctx->cmX = (double **)calloc(features, sizeof(double *));
    for(long i=0; i < features; i++){
      ctx->cmX[i] = (double *)calloc(samples, sizeof(double));
    }
  }

  ctx->allocatedfeatures = features;
  ctx->allocatedsamples = samples;
  ctx->features = features;
  ctx->samples = samples;
  // TODO: for sparsity,,, add more features ... 
}

void get_datainfo(FILE *fd, long *samples, long *features){

  char *ptr;
  long fcnt=0, scnt=0;
  char *tmpbuf = (char *)calloc(IOHANDLER_BUFLINE_MAX, sizeof(char));

  if(fgets(tmpbuf, IOHANDLER_BUFLINE_MAX, fd)){
    scnt++;
  }

  ptr = strtok(tmpbuf, INPUTDELIM);

  while(ptr != NULL){
    fcnt++;
    ptr = strtok(NULL, INPUTDELIM);
  }

  while(fgets(tmpbuf, IOHANDLER_BUFLINE_MAX, fd)){
    scnt++;
  }

  *samples = scnt;
  *features= fcnt;

  fseek(fd, 0L, SEEK_SET);

  strads_msg(INF, " %ld samples found\n", *samples);
  strads_msg(INF, " %ld features found\n", *features);
}


/* Standardize Matrix X by column */


void iohandler_read_fulldata(problemctx *pctx, int rank, int normalize, long unsigned int *randomcol){  

  char *ptr;
  long i, j;
  FILE *fdx, *fdy;
  char *linebuf;

  assert(pctx->X != NULL);
  assert(pctx->cmX != NULL);
  assert(pctx->Y != NULL);
  assert(pctx->pxsum != NULL);
  assert(pctx->pxsqsum != NULL);

  assert(pctx->features <= pctx->allocatedfeatures);
  assert(pctx->samples <= pctx->allocatedsamples);

  fdx = fopen(pctx->xfile, "r");
  fdy = fopen(pctx->yfile, "r");

  if(!fdx || !fdy){
    strads_msg(ERR, "fatal: %s %s files open failed \n", 
	    pctx->xfile, pctx->yfile);
    exit(0);
  }
  linebuf = (char *)calloc(IOHANDLER_BUFLINE_MAX, sizeof(char));
  assert(linebuf);

  i = 0;

  while(fgets(linebuf, IOHANDLER_BUFLINE_MAX, fdx) != NULL){
    j = 0;
    if(i >= pctx->samples){
      strads_msg(ERR, "fatal: input x has more samples \n");
      exit(0);
    }
    ptr = strtok(linebuf, INPUTDELIM);
    while(ptr != NULL){
      if(j >= pctx->features){
	strads_msg(ERR, "fatal: input x has more features \n");
	exit(0);
      }
      assert(randomcol[j] < (uint64_t)pctx->features);
      sscanf(ptr, "%lf", &pctx->X[i][randomcol[j]]);
      ptr = strtok(NULL, INPUTDELIM);
      j++;
    }
    assert(pctx->features == j);   
    i++;
  }
  assert(pctx->samples == i);


  
  /* alloc Y vector and read Y file */
  for(i=0; i<pctx->samples;i++){
    if(fgets(linebuf, IOHANDLER_BUFLINE_MAX, fdy) == NULL){
      printf("Input Matrix Y is not open linebuf[%s] i[%ld] samples:%ld\n", linebuf, i, pctx->samples);
      exit(0);
    }
    ptr = strtok(linebuf, INPUTDELIM);
    sscanf(ptr, "%lf", &pctx->Y[i]);
  }
  
  if(rank == 0)
    strads_msg(INF, "Y has [%ld] samples loading done\n", i);

  if(normalize == 1){
    strads_msg(ERR, "@@@  Read full data does normalization on X, Y ... \n");
    _standardizeInput(pctx->X, pctx->Y, pctx->samples, pctx->features);
    for(int i=0; i < pctx->features; i++){
      pctx->pxsum[i] = 0;
      pctx->pxsqsum[i] = 1;    
    }
  }else{
    for(int i =0; i < pctx->features; i++){
      double xsum=0;
      double xsqsum = 0;
      for(int j=0; j < pctx->samples; j++){
	xsum += pctx->X[j][i];
	xsqsum += (pctx->X[j][i]*pctx->X[j][i]);
      }
      pctx->pxsum[i] = xsum;
      pctx->pxsqsum[i] = xsqsum;
    }
    strads_msg(ERR, "@@@ NO normalization on X, Y ... \n");
  }

  free(linebuf);

  if(rank==0)
    strads_msg(ERR, "complete stadndization\n");
  return;
}

void iohandler_makecm(problemctx *cfg){
  long row, col;
  assert(cfg->cmflag == 1);
  assert(cfg->cmX != NULL); // check the sanity of mem allocation for column majored X
  assert(cfg->X != NULL);
  for(col=0; col < cfg->features; col++){
    if(cfg->cmX[col] == NULL){
      strads_msg(ERR, "mem alloc fatal: cmX[%ld] line is not allocated\n", col);
    }
  }
  for(row=0; row<cfg->samples; row++){
    for(col=0; col<cfg->features; col++){
      cfg->cmX[col][row] = cfg->X[row][col];
    }
  }
#if 0
  double sum, sqsum;
  for(row=0; row<cfg->features; row++){
    sum = 0;
    sqsum = 0;
    for(col=0; col<cfg->samples; col++){
      sum += cfg->cmX[row][col];
      sqsum += (cfg->cmX[row][col]*cfg->cmX[row][col]);
    }
    strads_msg(ERR, "cmX[%d]: sum %lf sqsum %lf\n", row, sum, sqsum);
  }
#endif 
}


void iohandler_getsysconf(const char *configfile, numactx *numainfo, problemctx *pctx, int rank){


  strads_msg(INF, "IOHANDLER.cpp Now machine conf is hard coded. Later replace it with reading config file %s\n", 
	     configfile);

  pctx->rhothreshold = RHOTHRESHOLD;
#if 1  // OpenCirrus, or VCLOUD
  numainfo->socket = 1;
  numainfo->corepersocket=8;
  numainfo->totalcores=8;
#endif 

#if 0  // SUSITA 
  numainfo->socket = 4;
  numainfo->corepersocket=16;
  numainfo->totalcores=64;
#endif 

#if 0
  FILE *fd = fopen(configfile, "r"); 
  if(fd == NULL){
    strads_msg(ERR, "fatal: can not open system configuration file \n");
    exit(0);   
  }
  char *linebuf = (char *)calloc(IOHANDLER_BUFLINE_MAX, 1);
  while(fgets(linebuf, IOHANDLER_BUFLINE_MAX) != NULL){
    /* some boost function recommended */
  }
  if(rank == 0)
    strads_msg(OUT, "system.conf file loading done\n");
  free(linebuf);
  fclose(fd);
#endif 
  return;
}

int iohandler_diskio_loadsubmat(dpartitionctx *dpart, long v_s, long v_len, long h_s, long h_len){  
  char *ptr;
  long col;
  long ploc=0, lloc=0;
  char *linebuf = (char *)calloc(IOHANDLER_BUFLINE_MAX, sizeof(char));

  rewind(dpart->fd);
  while(fgets(linebuf, IOHANDLER_BUFLINE_MAX, dpart->fd) != NULL){
    ploc++;
    if(ploc == v_s)break;
  }
  do{    
    col =0;
    ptr = strtok(linebuf, INPUTDELIM);
    while(ptr != NULL && col < (h_s + h_len)){

      if(col >= h_s)
	sscanf(ptr, "%lf", &dpart->row_mat[lloc][col-h_s]);
      ptr = strtok(NULL, INPUTDELIM);
    }    
    lloc++;
  }while((fgets(linebuf, IOHANDLER_BUFLINE_MAX, dpart->fd) != NULL) && (lloc <= v_len));
  return 1;
}





void iohandler_read_one_matrix(char *fn, double **X, uint64_t samples, uint64_t features, uint64_t *randrow, uint64_t *randcol, bool randflag){  

  uint64_t i, j;
  FILE *fdx;
  char *linebuf, *ptr;
  uint64_t tmprow, tmpcol;

  assert(features > 0);
  assert(samples > 0);
  fdx = fopen(fn, "r");
  assert(fdx);

  linebuf = (char *)calloc(IOHANDLER_BUFLINE_MAX, sizeof(char));
  assert(linebuf);

  i = 0;
  while(fgets(linebuf, IOHANDLER_BUFLINE_MAX, fdx) != NULL){
    j = 0;
    if(i >= samples){
      strads_msg(ERR, "fatal: input x has more samples \n");
      exit(0);
    }
    ptr = strtok(linebuf, INPUTDELIM);
    while(ptr != NULL){
      assert(j<features);
      if(randflag == true){
	tmprow = randrow[i];
	tmpcol = randcol[j];
      }else{
	tmprow = i;
	tmpcol = j;
      }     
      assert(tmprow < samples);
      assert(tmpcol < features);

      assert(tmprow >= 0);
      assert(tmpcol >= 0);
      
      //      sscanf(ptr, "%lf", &X[i][j]);
      sscanf(ptr, "%lf", &X[tmprow][tmpcol]);
      ptr = strtok(NULL, INPUTDELIM);
      j++;
    }
    assert(features == j); // should be equal to the number of features    
    i++;
  }
  assert(samples == i); // should be equal to the number of samples 
  return;
}



















/////////////////////////////////////////////////////////////////// 
// STATIC local funcation 
//////////////////////////////////////////////////////////////////

static void _standardizeInput(double **MX, double *MY, int n, int p){
  int i, j, row;
  double avg, dev, stdev, tmp;
  strads_msg(INF, "Standardize X/Y\n");
  /* standardize n-by-p X  column by colum */
  for(i=0; i<p; i++){
    avg = 0; 
    dev = 0;
    stdev = 0;
    for(j=0; j<n; j++){
      avg += MX[j][i];
    }
    avg = avg/n;
    dev = 0;
    for(j=0; j<n; j++){
      dev += ((MX[j][i]-avg)*(MX[j][i]-avg))/n;
    }    
    stdev = sqrt(dev);
    /* TODO: CHECK HOW TO DO STANDARDIZE WHEN STDEV == 0) */
    if(stdev == 0.0){
      for(row=0; row<n; row++){
	if(MX[row][i] != 0.0){
	  strads_msg(ERR, "fatal: stdev=0, MXelement nonzero, make [-nan]\n");
	  exit(0);
	}
      }
    }else{
      for(j=0; j<n; j++){
	tmp = ((MX[j][i]-avg)/stdev);      
	MX[j][i] = tmp/sqrt(n);      
      }
    }
  }

  /* normalize Y */
  avg = 0;
  dev = 0;
  stdev = 0;
  for(i=0; i<n; i++){
    avg += MY[i];
  }
  avg = avg/n;
  for(i=0; i<n; i++){
    dev += ((MY[i]-avg)*(MY[i]-avg))/n;
  }
  stdev = sqrt(dev);
  for(i=0; i<n; i++){
    tmp = ((MY[i]-avg)/stdev);
    MY[i] = tmp/sqrt(n);
  }


#if 0
  /* print out sum of each columns and sum of each colums squared */
  /* SUM 1 should be zero, SUM 2 should be 1 after normalization
   * (Standardization)
   */
  double sum1, sum2;
  for(i=0; i<p; i++){
    sum1 = 0; 
    sum2 = 0;
    for(j=0; j<n; j++){
      sum2 += (MX[j][i])*(MX[j][i]);
      sum1 += (MX[j][i]); 
    }
    strads_msg(ERR, "col[%d] sum %lf == 0.0 / square_sum %lf == 1.0 \n", i, sum1, sum2);
    if(fabs(sum1) > TOLERATE_PRECISION_ERROR){
      strads_msg(ERR, "\t sum shows error larger than precision error bound\n");
      strads_msg(ERR, "\t sum %2.20lf error bound %2.20lf\n", sum1, TOLERATE_PRECISION_ERROR); 
      exit(0);
    }

    if(fabs(sum2 -1) > TOLERATE_PRECISION_ERROR){
      strads_msg(ERR, "\t square sum shows error larger than precision error bound\n");
      strads_msg(ERR, "\t square sum %2.20lf \n", sum2); 
      exit(0);
    }
  }

  sum1 = 0;
  sum2 = 0;
  for(i=0; i<n; i++){
    strads_msg(ERR, "\n Y[%d] = [%lf] ",i, MY[i]);
    sum1 += (MY[i]);
    sum2 += (MY[i]*MY[i]);
  }
  strads_msg(ERR, "\n Y sum %2.20lf == 0.0 square_sum %2.20lf == 1.0\n", sum1, sum2);
  if(fabs(sum1) > TOLERATE_PRECISION_ERROR){
    strads_msg(ERR, "\t sum shows error larger than precision error bound\n");
    strads_msg(ERR, "\t sum %2.20lf \n", sum1); 
    exit(0);
  }

  if(fabs(sum2 -1) > TOLERATE_PRECISION_ERROR){
    strads_msg(ERR, "\t square sum shows error larger than precision error bound\n");
    strads_msg(ERR, "\t square sum %2.20lf \n", sum2);  
    exit(0);
  }
#endif 
}
