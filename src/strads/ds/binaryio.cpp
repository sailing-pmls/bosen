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
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <math.h>
#include <openssl/md5.h>
#include <openssl/hmac.h>
#include <assert.h>
#include "binaryio.hpp"
#include "./include/utility.hpp"

using namespace std;
//#define MD5VERIFY
long iohandler_spmat_pbfio_read_size(const char *fn, uint64_t *sample, uint64_t *feature, uint64_t *nz){
  int rfd, readbytes;
  pbfheader *fh = (pbfheader *)calloc(1, sizeof(pbfheader));
  if((rfd = open(fn, O_RDONLY)) == -1){
    exit(-1);
  }
  readbytes = pbfio_read_bytes(rfd, 0, sizeof(pbfheader), (void *)fh);  
  assert(readbytes == sizeof(pbfheader));
  *sample = fh->maxrow;
  *feature = fh->maxcol;
  *nz = fh->nonzero;  
  close(rfd);
  return 0;
}

int pbfio_read_bytes(int rfd, int64_t offset,  size_t bytes, void *into){
  int ret;
  int64_t lret;
  //printf("Reading sectors %"PRId64"--%"PRId64"\n", block, block + (numsectors-1));
  if ((lret = lseek64(rfd, offset, SEEK_SET)) != offset) {
    fprintf(stderr, "Seek to position %ld failed: returned %ld\n", 
	    offset, lret);
    exit(-1);
  }
  ret = read(rfd, into, bytes);
  return ret;
}

void pbfio_write_bytes(int wfd, int64_t offset, size_t bytes, void *from){
  int ret;
  int64_t lret;
  if ((lret = lseek64(wfd, offset, SEEK_SET)) != offset) {
    fprintf(stderr, "Seek to position %ld failed: returned %ld\n", 
	    offset, lret);
    exit(-1);
  }
  if ((ret = write(wfd, from, bytes)) != static_cast<int>(bytes)) {
    fprintf(stderr, "Write %ld bytes failed: returned %d\n", bytes, ret);
    exit(-1);
  }  
}

bool _range_sanity_check(uint64_t frow_s, uint64_t frow_end, uint64_t fcol_s, uint64_t fcol_end, 
			 uint64_t drow_s, uint64_t drow_end, uint64_t dcol_s, uint64_t dcol_end){
  strads_msg(INF, "row_s(%ld)     drow_s(%ld)", frow_s, drow_s);
  strads_msg(INF, "row_end(%ld)   drow_end(%ld)", frow_end, drow_end);  
  assert(frow_s <= drow_s);
  assert(frow_end >= drow_end);
  assert(fcol_s <= dcol_s);
  assert(fcol_end >= fcol_end);
  return true;
}


long iohandler_spmat_pbfio_partialread(dshardctx *dshard, bool matlabflag, int rank){

#if defined(MD5VERIFY)
  MD5_CTX c;
  uint8_t out[MD5_DIGEST_LENGTH];
  MD5_Init(&c);
#endif 
  pbfheader *fh = (pbfheader *)calloc(1, sizeof(pbfheader));
  int64_t cpos = sizeof(pbfheader);
  uint64_t toread = PBFIO_ENTRY_TO_READ;
  nzentry *nzbuf = (nzentry *)calloc(toread, sizeof(nzentry));
  uint64_t nzcnt=0;
  int rfd;
  uint64_t tmprow, tmpcol;
  double tmpval;
  int readbytes;

  strads_msg(ERR, "partialRead :rank(%d) file(%s), rs(%ld) re(%ld) rlen(%ld) cs(%ld) ce(%ld) clen(%ld) MAJORTYPE(%d) (legend: rowmajor:%d, colmajor:%d)\n", 
	     rank, dshard->m_fn, dshard->m_range.r_start, dshard->m_range.r_end, dshard->m_range.r_len,  
	     dshard->m_range.c_start, dshard->m_range.c_end, dshard->m_range.c_len, dshard->m_type, rmspt, cmspt);

  if((rfd = open(dshard->m_fn, O_RDONLY)) == -1){
    strads_msg(ERR, "fatal: mach(%d) data partition file(%s) open fail\n", 
	       rank, dshard->m_fn);
    exit(-1);
  }
  readbytes = pbfio_read_bytes(rfd, 0, sizeof(pbfheader), (void *)fh);
  uint64_t fmaxrow = fh->maxrow;
  uint64_t fmaxcol = fh->maxcol;
  uint64_t frow_s = fh->row_s;
  uint64_t frow_end = fh->row_end;
  uint64_t fcol_s = fh->col_s;
  uint64_t fcol_end = fh->col_end;
  strads_msg(INF, "From FILE frow_s(%ld) frow_end(%ld) fcol_s(%ld) fcol_end(%ld)\n", 
	     frow_s, frow_end, fcol_s, fcol_end);
  _range_sanity_check(frow_s, frow_end, fcol_s, fcol_end, 
		      dshard->m_range.r_start, dshard->m_range.r_end, 
		      dshard->m_range.c_start, dshard->m_range.c_end);   
  strads_msg(OUT, "@@@@@ binaryio.cpp in the header: %ld nzcnt to read\n", fh->nonzero);
  uint64_t entrycnt=0;
  uint64_t local_nzcnt = 0;

  while(1){

    readbytes = pbfio_read_bytes(rfd, cpos, toread*sizeof(nzentry), (void *)nzbuf);
    assert((readbytes%sizeof(nzentry)) == 0);
    entrycnt = readbytes/sizeof(nzentry);
    nzcnt += entrycnt;

    cpos += readbytes;
    for(uint64_t i=0; i<entrycnt; i++){
      tmprow = nzbuf[i].row;
      tmpcol = nzbuf[i].col;
      tmpval = nzbuf[i].val;               
      if(0){
	strads_msg(ERR, " tmprow(%lu)  tmpcol(%lu) tmpval(%lf)\n", 
		   tmprow, tmpcol, tmpval);
      }
      if(matlabflag){
	tmprow--;  /* adjust from 1-based to 0-based */
	tmpcol--;    
      }

      if(!(tmprow < fmaxrow)){
	strads_msg(ERR, "\n tmprow (%lu) maxrow(%lu)\n", 
		   tmprow, fmaxrow);
	exit(0);
      }
      if(!(tmpcol < fmaxcol)){
	strads_msg(ERR, "\n tmpcol (%lu) maxcol(%lu)\n", 
		   tmpcol, fmaxcol);
	exit(0);
      }

      assert(tmprow < fmaxrow);
      assert(tmpcol < fmaxcol);

      if((tmprow >= (long unsigned int)dshard->m_range.r_start) 
	 && (tmprow <= (long unsigned int)dshard->m_range.r_end)
	 && (tmpcol >= (long unsigned int)dshard->m_range.c_start)
	 && (tmpcol <= (long unsigned int)dshard->m_range.c_end)) {

	if(dshard->m_type == rmspt){
	  strads_msg(INF, "Row Major (%ld, %ld) = %lf", tmprow, tmpcol, tmpval);
	  dshard->m_rm_mat(tmprow, tmpcol) = tmpval;

	}else if(dshard->m_type == cmspt){ // column major 
	  strads_msg(INF, "Col Major (%ld, %ld) = %lf", tmprow, tmpcol, tmpval);
	  dshard->m_cm_mat(tmprow, tmpcol) = tmpval;
	}else if(dshard->m_type == d2dmat){
	  dshard->m_dmat.m_mem[tmprow][tmpcol] = tmpval;
#if defined(TEST_ATOMIC)
	  // only for residual test 
	  assert(tmpcol == 0);
	  dshard->m_atomic[tmprow] = tmpval;
#endif
	  //	  strads_msg(INF, "Col Major Vector (%ld, %ld) = %lf", tmprow, tmpcol, tmpval);
	  //	  dshard->m_cm_vmat(tmprow, tmpcol) = tmpval;
	}else if(dshard->m_type == rvspt){
	  dshard->m_rm_vmat.add(tmprow, tmpcol, tmpval);
	}else if(dshard->m_type == cvspt){
	  dshard->m_cm_vmat.add(tmprow, tmpcol, tmpval);
	}else{
	  strads_msg(ERR, "Current version support rm/cm spmat only\n");
	  assert(0);
	  // URGENT DO: TODO: add more type here vmat, dmat.  
	}
	local_nzcnt++;
	if(nzcnt %10000 == 0){
	  strads_msg(INF, " %ld th nzelement at rank (%d)", nzcnt, rank);
	}
      }
    }
#if defined(MD5VERIFY)
    MD5_Update(&c, (uint8_t *)nzbuf, readbytes);
#endif 
    if(readbytes != (long)(toread*sizeof(nzentry))){
      break;
    }
  } // while (1)

#if defined(MD5VERIFY)
  MD5_Final(out, &c);
  for(uint64_t n=0; n < MD5_DIGEST_LENGTH; n++){
    assert(out[n] == fh->md5key[n]);
  }
  strads_msg(ERR, "MD5Hash Key match with one in the disk imsage\n");
#endif 

  strads_msg(ERR, "\n\n\n in My local  binaryio.cpp: localnz: (%ld) all %ld\n\n\n\n", local_nzcnt, nzcnt);
  close(rfd);
  return 0;
}


long iohandler_spmat_pbfio_partialread_sortedorder(dshardctx *dshard, bool matlabflag, int rank, sortingtype sortbase){

#if defined(MD5VERIFY)
  MD5_CTX c;
  uint8_t out[MD5_DIGEST_LENGTH];
  MD5_Init(&c);
#endif 
  pbfheader *fh = (pbfheader *)calloc(1, sizeof(pbfheader));
  int64_t cpos = sizeof(pbfheader);
  uint64_t toread = PBFIO_ENTRY_TO_READ;
  nzentry *nzbuf = (nzentry *)calloc(toread, sizeof(nzentry));
  uint64_t nzcnt=0;
  int rfd;
  uint64_t tmprow, tmpcol;
  double tmpval;
  int readbytes;

  strads_msg(ERR, "[@@@@@@@] partialRead :rank(%d) file(%s), rs(%ld) re(%ld) rlen(%ld) cs(%ld) ce(%ld) clen(%ld) MAJORTYPE(%d) (legend: rowmajor:%d, colmajor:%d)\n", 
	     rank, dshard->m_fn, dshard->m_range.r_start, dshard->m_range.r_end, dshard->m_range.r_len,  
	     dshard->m_range.c_start, dshard->m_range.c_end, dshard->m_range.c_len, dshard->m_type, rmspt, cmspt);

  if((rfd = open(dshard->m_fn, O_RDONLY)) == -1){
    strads_msg(ERR, "fatal: mach(%d) data partition file(%s) open fail\n", 
	       rank, dshard->m_fn);
    exit(-1);
  }

  readbytes = pbfio_read_bytes(rfd, 0, sizeof(pbfheader), (void *)fh);

  uint64_t fmaxrow = fh->maxrow;
  uint64_t fmaxcol = fh->maxcol;
  uint64_t frow_s = fh->row_s;
  uint64_t frow_end = fh->row_end;
  uint64_t fcol_s = fh->col_s;
  uint64_t fcol_end = fh->col_end;

  strads_msg(ERR, "@@@@@ From FILE frow_s(%ld) frow_end(%ld) fcol_s(%ld) fcol_end(%ld)\n", 
	     frow_s, frow_end, fcol_s, fcol_end);

  _range_sanity_check(frow_s, frow_end, fcol_s, fcol_end, 
		      dshard->m_range.r_start, dshard->m_range.r_end, 
		      dshard->m_range.c_start, dshard->m_range.c_end);   
  strads_msg(ERR, "@@@@@ binaryio.cpp in the header: %ld nzcnt to read\n", fh->nonzero);
  uint64_t entrycnt=0;
  uint64_t local_nzcnt = 0;

  while(1){
    readbytes = pbfio_read_bytes(rfd, cpos, toread*sizeof(nzentry), (void *)nzbuf);
    assert((readbytes%sizeof(nzentry)) == 0);
    entrycnt = readbytes/sizeof(nzentry);
    nzcnt += entrycnt;
    cpos += readbytes;
    for(uint64_t i=0; i<entrycnt; i++){
      tmprow = nzbuf[i].row;
      tmpcol = nzbuf[i].col;
      tmpval = nzbuf[i].val;               
      if(0){
	strads_msg(ERR, " tmprow(%lu)  tmpcol(%lu) tmpval(%lf)\n", 
		   tmprow, tmpcol, tmpval);
      }

      if(matlabflag){
	tmprow--;  /* adjust from 1-based to 0-based */
	tmpcol--;    
      }

      if(!(tmprow < fmaxrow)){
	strads_msg(ERR, "\n tmprow (%lu) maxrow(%lu)\n", 
		   tmprow, fmaxrow);
	exit(0);
      }
      if(!(tmpcol < fmaxcol)){
	strads_msg(ERR, "\n tmpcol (%lu) maxcol(%lu)\n", 
		   tmpcol, fmaxcol);
	exit(0);
      }

      assert(tmprow < fmaxrow);
      assert(tmpcol < fmaxcol);
      if((tmprow >= (long unsigned int)dshard->m_range.r_start) 
	 && (tmprow <= (long unsigned int)dshard->m_range.r_end)
	 && (tmpcol >= (long unsigned int)dshard->m_range.c_start)
	 && (tmpcol <= (long unsigned int)dshard->m_range.c_end)) {

	if(dshard->m_type == rmspt){
	  strads_msg(INF, "Row Major (%ld, %ld) = %lf", tmprow, tmpcol, tmpval);
	  dshard->m_rm_mat(tmprow, tmpcol) = tmpval;

	}else if(dshard->m_type == cmspt){ // column major 
	  strads_msg(INF, "Col Major (%ld, %ld) = %lf", tmprow, tmpcol, tmpval);
	  dshard->m_cm_mat(tmprow, tmpcol) = tmpval;
	}else if(dshard->m_type == d2dmat){
	  dshard->m_dmat.m_mem[tmprow][tmpcol] = tmpval;
#if defined(TEST_ATOMIC)
	  // only for residual test 
	  assert(tmpcol == 0);
	  dshard->m_atomic[tmprow] = tmpval;
#endif
	  //	  strads_msg(INF, "Col Major Vector (%ld, %ld) = %lf", tmprow, tmpcol, tmpval);
	  //	  dshard->m_cm_vmat(tmprow, tmpcol) = tmpval;
	}else if(dshard->m_type == rvspt){
	  dshard->m_rm_vmat.add(tmprow, tmpcol, tmpval);
	}else if(dshard->m_type == cvspt){
	  // TODO : generalize, 
	  // CAVEAT now only for colum major one doing sorted row insert 
	  dshard->m_cm_vmat.add_with_row_sorting(tmprow, tmpcol, tmpval);
	  //	  dshard->m_cm_vmat.add(tmprow, tmpcol, tmpval);
	}else{
	  strads_msg(ERR, "Current version support rm/cm spmat only\n");
	  assert(0);
	  // URGENT DO: TODO: add more type here vmat, dmat.  
	}
	local_nzcnt++;
	if(nzcnt %10000 == 0){
	  strads_msg(INF, " %ld th nzelement at rank (%d)", nzcnt, rank);
	}
      }
    }
#if defined(MD5VERIFY)
    MD5_Update(&c, (uint8_t *)nzbuf, readbytes);
#endif 
    if(readbytes != (long)(toread*sizeof(nzentry))){
      break;
    }
  } // while (1)

#if defined(MD5VERIFY)
  MD5_Final(out, &c);
  for(uint64_t n=0; n < MD5_DIGEST_LENGTH; n++){
    assert(out[n] == fh->md5key[n]);
  }
  strads_msg(ERR, "MD5Hash Key match with one in the disk imsage\n");
#endif 

  strads_msg(ERR, "\n\n\n in My local  binaryio.cpp: localnz: (%ld) all %ld\n\n\n\n", local_nzcnt, nzcnt);

  close(rfd);
  return 0;
}

long iohandler_spmat_pbfio_partialread(dshardctx *dshard, bool matlabflag, int rank, sharedctx *ctx){

  bool permute_flag = ctx->m_params->m_sp->m_apply_permute_col;
  int64_t *col_permute_map = ctx->m_model_permute;
  // TODO : D2MAT is Y reading. DO not column remaping for d2dmat 
  if(permute_flag and dshard->m_type != d2dmat )
    strads_msg(ERR, "WARNING: PERMUTE ON COL IS APPLIED THIS IS NOT FOR SVM \n");

#if defined(MD5VERIFY)
  MD5_CTX c;
  uint8_t out[MD5_DIGEST_LENGTH];
  MD5_Init(&c);
#endif 
  pbfheader *fh = (pbfheader *)calloc(1, sizeof(pbfheader));
  int64_t cpos = sizeof(pbfheader);
  uint64_t toread = PBFIO_ENTRY_TO_READ;
  nzentry *nzbuf = (nzentry *)calloc(toread, sizeof(nzentry));
  uint64_t nzcnt=0;
  int rfd;
  uint64_t tmprow, tmpcol;
  double tmpval;
  int readbytes;

  strads_msg(ERR, "partialRead :rank(%d) file(%s), rs(%ld) re(%ld) rlen(%ld) cs(%ld) ce(%ld) clen(%ld) MAJORTYPE(%d) (legend: rowmajor:%d, colmajor:%d)\n", 
	     rank, dshard->m_fn, dshard->m_range.r_start, dshard->m_range.r_end, dshard->m_range.r_len,  
	     dshard->m_range.c_start, dshard->m_range.c_end, dshard->m_range.c_len, dshard->m_type, rmspt, cmspt);

  if((rfd = open(dshard->m_fn, O_RDONLY)) == -1){
    strads_msg(ERR, "fatal: mach(%d) data partition file(%s) open fail\n", 
	       rank, dshard->m_fn);
    exit(-1);
  }

  readbytes = pbfio_read_bytes(rfd, 0, sizeof(pbfheader), (void *)fh);

  uint64_t fmaxrow = fh->maxrow;
  uint64_t fmaxcol = fh->maxcol;
  uint64_t frow_s = fh->row_s;
  uint64_t frow_end = fh->row_end;
  uint64_t fcol_s = fh->col_s;
  uint64_t fcol_end = fh->col_end;

  strads_msg(ERR, "from FILE frow_s(%ld) frow_end(%ld) fcol_s(%ld) fcol_end(%ld)\n", 
	     frow_s, frow_end, fcol_s, fcol_end);

  _range_sanity_check(frow_s, frow_end, fcol_s, fcol_end, 
		      dshard->m_range.r_start, dshard->m_range.r_end, 
		      dshard->m_range.c_start, dshard->m_range.c_end); 
  
  strads_msg(ERR, "binaryio.cpp in the header: %ld nzcnt to read\n", fh->nonzero);

  uint64_t entrycnt=0;
  uint64_t local_nzcnt = 0;

  while(1){

    readbytes = pbfio_read_bytes(rfd, cpos, toread*sizeof(nzentry), (void *)nzbuf);
    assert((readbytes%sizeof(nzentry)) == 0);
    entrycnt = readbytes/sizeof(nzentry);
    nzcnt += entrycnt;

    cpos += readbytes;
    for(uint64_t i=0; i<entrycnt; i++){
      tmprow = nzbuf[i].row;
      tmpcol = nzbuf[i].col;
      tmpval = nzbuf[i].val;               
      if(0){
	strads_msg(ERR, " tmprow(%lu)  tmpcol(%lu) tmpval(%lf)\n", 
		   tmprow, tmpcol, tmpval);
      }
      if(matlabflag){
	tmprow--;  /* adjust from 1-based to 0-based */
	tmpcol--;    
      }

      // TODO : D2MAT is Y reading. DO not column remaping for d2dmat 
      if(permute_flag and dshard->m_type != d2dmat){
	int64_t remapcol = col_permute_map[tmpcol];
	tmpcol = remapcol;
      }


      if(!(tmprow < fmaxrow)){
	strads_msg(ERR, "\n tmprow (%lu) maxrow(%lu)\n", 
		   tmprow, fmaxrow);
	exit(0);
      }
      if(!(tmpcol < fmaxcol)){
	strads_msg(ERR, "\n tmpcol (%lu) maxcol(%lu)\n", 
		   tmpcol, fmaxcol);
	exit(0);
      }

      assert(tmprow < fmaxrow);
      assert(tmpcol < fmaxcol);
      if((tmprow >= (long unsigned int)dshard->m_range.r_start) 
	 && (tmprow <= (long unsigned int)dshard->m_range.r_end)
	 && (tmpcol >= (long unsigned int)dshard->m_range.c_start)
	 && (tmpcol <= (long unsigned int)dshard->m_range.c_end)) {

	if(dshard->m_type == rmspt){
	  strads_msg(INF, "Row Major (%ld, %ld) = %lf", tmprow, tmpcol, tmpval);
	  dshard->m_rm_mat(tmprow, tmpcol) = tmpval;

	}else if(dshard->m_type == cmspt){ // column major 
	  strads_msg(INF, "Col Major (%ld, %ld) = %lf", tmprow, tmpcol, tmpval);
	  dshard->m_cm_mat(tmprow, tmpcol) = tmpval;
	}else if(dshard->m_type == d2dmat){
	  dshard->m_dmat.m_mem[tmprow][tmpcol] = tmpval;
#if defined(TEST_ATOMIC)
	  // only for residual test 
	  assert(tmpcol == 0);
	  dshard->m_atomic[tmprow] = tmpval;
#endif
	}else if(dshard->m_type == rvspt){
	  dshard->m_rm_vmat.add(tmprow, tmpcol, tmpval);
	}else if(dshard->m_type == cvspt){
	  dshard->m_cm_vmat.add(tmprow, tmpcol, tmpval);
	}else{
	  strads_msg(ERR, "Current version support rm/cm spmat only\n");
	  assert(0);
	  // URGENT DO: TODO: add more type here vmat, dmat.  
	}

	local_nzcnt++;
	if(nzcnt %10000 == 0){
	  strads_msg(INF, " %ld th nzelement at rank (%d)", nzcnt, rank);
	}
      }

    }
#if defined(MD5VERIFY)
    MD5_Update(&c, (uint8_t *)nzbuf, readbytes);
#endif 
    if(readbytes != (long)(toread*sizeof(nzentry))){
      break;
    }
  } // while (1)

#if defined(MD5VERIFY)
  MD5_Final(out, &c);
  for(uint64_t n=0; n < MD5_DIGEST_LENGTH; n++){
    assert(out[n] == fh->md5key[n]);
  }
  strads_msg(ERR, "MD5Hash Key match with one in the disk imsage\n");
#endif 
  strads_msg(ERR, "\n\n\n in My local  binaryio.cpp: localnz: (%ld) all %ld\n\n\n\n", local_nzcnt, nzcnt);
  close(rfd);
  return 0;
}


long iohandler_spmat_pbfio_partialread_sortedorder(dshardctx *dshard, bool matlabflag, int rank, sortingtype sortbase, sharedctx *ctx){

  bool permute_flag = ctx->m_params->m_sp->m_apply_permute_col;
  int64_t *col_permute_map = ctx->m_model_permute;

  // TODO : D2MAT is Y reading. DO not column remaping for d2dmat 
  if(permute_flag and dshard->m_type != d2dmat )
    strads_msg(ERR, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ WARNING: PERMUTE ON COL IS APPLIED THIS IS NOT FOR SVM \n");

#if defined(MD5VERIFY)
  MD5_CTX c;
  uint8_t out[MD5_DIGEST_LENGTH];
  MD5_Init(&c);
#endif 
  pbfheader *fh = (pbfheader *)calloc(1, sizeof(pbfheader));
  int64_t cpos = sizeof(pbfheader);
  uint64_t toread = PBFIO_ENTRY_TO_READ;
  nzentry *nzbuf = (nzentry *)calloc(toread, sizeof(nzentry));
  uint64_t nzcnt=0;
  int rfd;
  uint64_t tmprow, tmpcol;
  double tmpval;
  int readbytes;

  strads_msg(ERR, "[@@@@@@@] partialRead :rank(%d) file(%s), rs(%ld) re(%ld) rlen(%ld) cs(%ld) ce(%ld) clen(%ld) MAJORTYPE(%d) (legend: rowmajor:%d, colmajor:%d)\n", 
	     rank, dshard->m_fn, dshard->m_range.r_start, dshard->m_range.r_end, dshard->m_range.r_len,  
	     dshard->m_range.c_start, dshard->m_range.c_end, dshard->m_range.c_len, dshard->m_type, rmspt, cmspt);

  if((rfd = open(dshard->m_fn, O_RDONLY)) == -1){
    strads_msg(ERR, "fatal: mach(%d) data partition file(%s) open fail\n", 
	       rank, dshard->m_fn);
    exit(-1);
  }

  readbytes = pbfio_read_bytes(rfd, 0, sizeof(pbfheader), (void *)fh);
  uint64_t fmaxrow = fh->maxrow;
  uint64_t fmaxcol = fh->maxcol;
  uint64_t frow_s = fh->row_s;
  uint64_t frow_end = fh->row_end;
  uint64_t fcol_s = fh->col_s;
  uint64_t fcol_end = fh->col_end;
  strads_msg(ERR, "@@@@@ From FILE frow_s(%ld) frow_end(%ld) fcol_s(%ld) fcol_end(%ld)\n", 
	     frow_s, frow_end, fcol_s, fcol_end);

  _range_sanity_check(frow_s, frow_end, fcol_s, fcol_end, 
		      dshard->m_range.r_start, dshard->m_range.r_end, 
		      dshard->m_range.c_start, dshard->m_range.c_end); 
  
  strads_msg(ERR, "@@@@@ binaryio.cpp in the header: %ld nzcnt to read\n", fh->nonzero);

  uint64_t entrycnt=0;
  uint64_t local_nzcnt = 0;

  while(1){

    readbytes = pbfio_read_bytes(rfd, cpos, toread*sizeof(nzentry), (void *)nzbuf);
    assert((readbytes%sizeof(nzentry)) == 0);
    entrycnt = readbytes/sizeof(nzentry);
    nzcnt += entrycnt;

    cpos += readbytes;
    for(uint64_t i=0; i<entrycnt; i++){

      tmprow = nzbuf[i].row;
      tmpcol = nzbuf[i].col;
      tmpval = nzbuf[i].val;               

      if(matlabflag){
	tmprow--;  /* adjust from 1-based to 0-based */
	tmpcol--;    
      }

      // TODO : D2MAT is Y reading. DO not column remaping for d2dmat 
      if(permute_flag and dshard->m_type != d2dmat ){
	int64_t remapcol = col_permute_map[tmpcol];
	tmpcol = remapcol;
      }

      if(!(tmprow < fmaxrow)){
	strads_msg(ERR, "\n tmprow (%lu) maxrow(%lu)\n", 
		   tmprow, fmaxrow);
	exit(0);
      }
      if(!(tmpcol < fmaxcol)){
	strads_msg(ERR, "\n tmpcol (%lu) maxcol(%lu)\n", 
		   tmpcol, fmaxcol);
	exit(0);
      }

      assert(tmprow < fmaxrow);
      assert(tmpcol < fmaxcol);
      if((tmprow >= (long unsigned int)dshard->m_range.r_start) 
	 && (tmprow <= (long unsigned int)dshard->m_range.r_end)
	 && (tmpcol >= (long unsigned int)dshard->m_range.c_start)
	 && (tmpcol <= (long unsigned int)dshard->m_range.c_end)) {

	if(dshard->m_type == rmspt){
	  strads_msg(INF, "Row Major (%ld, %ld) = %lf", tmprow, tmpcol, tmpval);
	  dshard->m_rm_mat(tmprow, tmpcol) = tmpval;

	}else if(dshard->m_type == cmspt){ // column major 
	  strads_msg(INF, "Col Major (%ld, %ld) = %lf", tmprow, tmpcol, tmpval);
	  dshard->m_cm_mat(tmprow, tmpcol) = tmpval;
	}else if(dshard->m_type == d2dmat){
	  dshard->m_dmat.m_mem[tmprow][tmpcol] = tmpval;

#if defined(TEST_ATOMIC)
	  // only for residual test 
	  assert(tmpcol == 0);
	  dshard->m_atomic[tmprow] = tmpval;
#endif
	}else if(dshard->m_type == rvspt){
	  dshard->m_rm_vmat.add(tmprow, tmpcol, tmpval);
	}else if(dshard->m_type == cvspt){
	  dshard->m_cm_vmat.add_with_row_sorting(tmprow, tmpcol, tmpval);
	}else{
	  strads_msg(ERR, "Current version support rm/cm spmat only\n");
	  assert(0);
	}
	local_nzcnt++;
	if(nzcnt %10000 == 0){
	  strads_msg(INF, " %ld th nzelement at rank (%d)", nzcnt, rank);
	}
      }

    }
#if defined(MD5VERIFY)
    MD5_Update(&c, (uint8_t *)nzbuf, readbytes);
#endif 
    if(readbytes != (long)(toread*sizeof(nzentry))){
      break;
    }
  } // while (1)

#if defined(MD5VERIFY)
  MD5_Final(out, &c);
  for(uint64_t n=0; n < MD5_DIGEST_LENGTH; n++){
    assert(out[n] == fh->md5key[n]);
  }
  strads_msg(ERR, "MD5Hash Key match with one in the disk imsage\n");
#endif 

  strads_msg(ERR, "\n\n\n in My local  binaryio.cpp: localnz: (%ld) all %ld\n\n\n\n", local_nzcnt, nzcnt);
  close(rfd);
  return 0;
}
