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
//#include <openssl/md5.h>
//#include <openssl/hmac.h>
#include <assert.h>
#include "binaryio.hpp"
#include "dpartitioner.hpp"
#include "utility.hpp"

using namespace std;

//#define MD5VERIFY

bool _range_sanity_check(uint64_t frow_s, uint64_t frow_end, uint64_t fcol_s, uint64_t fcol_end, 
			 uint64_t drow_s, uint64_t drow_end, uint64_t dcol_s, uint64_t dcol_end);

bool prange_checking(uint64_t tmpcol, param_range *selected_prange, int selected_prcnt){
  for(int i=0; i<selected_prcnt; i++){
    if(tmpcol >= selected_prange[i].feature_start &&
       tmpcol <= selected_prange[i].feature_end)
      return true;
  }
  return false; // if tmpcol does not belong to any selected prange. return false
}

long iohandler_spmat_pbfio_partial_read_wt_ooc(char *fn, dpartitionctx *dpart, bool matlabflag, int rank, param_range *selected_prange, int selected_prcnt){

  pbfheader *fh = (pbfheader *)calloc(1, sizeof(pbfheader));
  int64_t cpos = sizeof(pbfheader);
  uint64_t toread = PBFIO_ENTRY_TO_READ;
  nzentry *nzbuf = (nzentry *)calloc(toread, sizeof(nzentry));
  uint64_t nzcnt=0;
  int rfd;
  uint64_t tmprow, tmpcol;
  double tmpval;
  int readbytes;

  strads_msg(INF, "iohandler_partialread_wt_ooc:rank(%d)fn(%s), v_s(%ld) v_end(%ld) v_len(%ld) h_S(%ld) h_end(%ld) h_len(%ld)\n",
	     rank, fn, dpart->range.v_s, dpart->range.v_end, dpart->range.v_len,  
	     dpart->range.h_s, dpart->range.h_end, dpart->range.h_len);

  assert(selected_prange);
  for(int i=0; i < selected_prcnt; i++){
    strads_msg(INF, "pbio-partial-read-wt-ooc selected_pragne[%d] colstart(%ld) colend(%ld) \n", 
	       selected_prange[i].gid, 
	       selected_prange[i].feature_start, 
	       selected_prange[i].feature_end);
  }

  // open file 
  if((rfd = open(dpart->fn, O_RDONLY)) == -1){
    strads_msg(ERR, "fatal: mach(%d) data partition file(%s) open fail\n", 
	       rank, dpart->fn);
    exit(-1);
  }

  readbytes = pbfio_read_bytes(rfd, 0, sizeof(pbfheader), (void *)fh);

  uint64_t fmaxrow = fh->maxrow;
  uint64_t fmaxcol = fh->maxcol;
  uint64_t frow_s = fh->row_s;
  uint64_t frow_end = fh->row_end;
  uint64_t fcol_s = fh->col_s;
  uint64_t fcol_end = fh->col_end;

  strads_msg(ERR, "@@@@@@@@@@@@@@@ frow_s(%ld) frow_end(%ld) fcol_s(%ld) fcol_end(%ld)\n", 
	     frow_s, frow_end, fcol_s, fcol_end);

  _range_sanity_check(frow_s, frow_end, fcol_s, fcol_end, 
		      dpart->range.v_s, dpart->range.v_end, 
		      dpart->range.h_s, dpart->range.h_end); 

  // TODO : add prnage sanity checker .... 
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

      bool prangeflag = prange_checking(tmpcol, selected_prange, selected_prcnt);

      if((tmprow >= (long unsigned int)dpart->range.v_s) 
	 && (tmprow <= (long unsigned int)dpart->range.v_end)
	 && (tmpcol >= (long unsigned int)dpart->range.h_s)
	 && (tmpcol <= (long unsigned int)dpart->range.h_end)
	 && (prangeflag)) {

	if(dpart->dparttype == rowmajor_matrix){
	  strads_msg(INF, "Row Major (%ld, %ld) = %lf", tmprow, tmpcol, tmpval);
	  dpart->sprow_mat(tmprow, tmpcol) = tmpval;
	}else{ // column major 

	  strads_msg(INF, "Col Major (%ld, %ld) = %lf", tmprow, tmpcol, tmpval);
	  dpart->spcol_mat(tmprow, tmpcol) = tmpval;
	}	
	local_nzcnt++;
	if(nzcnt %10000 == 0){
	  strads_msg(INF, " %ld th nzelement at rank (%d)", nzcnt, rank);
	}
      }

    }
    if(readbytes != (long)(toread*sizeof(nzentry))){
      break;
    }
  } // while (1)

  strads_msg(ERR, "\n\n\nbinaryio.cpp: localnz: (%ld) all %ld\n\n\n\n", local_nzcnt, nzcnt);
  free(nzbuf);
  free(fh);
  close(rfd);
  return 0;
}





















































long iohandler_spmat_pbfio_read_size(char *fn, uint64_t *sample, uint64_t *feature, uint64_t *nz){

  int rfd, readbytes;
  pbfheader *fh = (pbfheader *)calloc(1, sizeof(pbfheader));
  if((rfd = open(fn, O_RDONLY)) == -1){
    //    strads_msg(ERR, "fatal: mach(%d) data partition file(%s) open fail\n", 
    //	       rank, fn);
    exit(-1);
  }
  readbytes = pbfio_read_bytes(rfd, 0, sizeof(pbfheader), (void *)fh);  
  assert(readbytes == sizeof(pbfheader));
  *sample = fh->maxrow;
  *feature = fh->maxcol;
  *nz = fh->nonzero;  


  free(fh);
  close(rfd);
  return 0;
}

int pbfio_read_bytes(int rfd, int64_t offset,  size_t bytes, void *into)
{
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

void pbfio_write_bytes(int wfd, int64_t offset, size_t bytes, void *from)
{
  int ret;
  int64_t lret;
  if ((lret = lseek64(wfd, offset, SEEK_SET)) != offset) {
    fprintf(stderr, "Seek to position %ld failed: returned %ld\n", 
	    offset, lret);
    exit(-1);
  }
  if ((ret = write(wfd, from, bytes)) != bytes) {
    fprintf(stderr, "Write %ld bytes failed: returned %d\n", bytes, ret);
    exit(-1);
  }  
}


bool _range_sanity_check(uint64_t frow_s, uint64_t frow_end, uint64_t fcol_s, uint64_t fcol_end, 
			 uint64_t drow_s, uint64_t drow_end, uint64_t dcol_s, uint64_t dcol_end){

  strads_msg(ERR, "@@@@@@@@@@@@@@@@@ frow_s(%ld)   drow_s(%ld)", frow_s, drow_s);
  strads_msg(ERR, "@@@@@@@@@@@@@@@@@ frow_end(%ld)   drow_end(%ld)", frow_end, drow_end);
  
  assert(frow_s <= drow_s);
  assert(frow_end >= drow_end);
  assert(fcol_s <= dcol_s);
  assert(fcol_end >= fcol_end);

  return true;
}

long iohandler_spmat_pbfio_partial_read(char *fn, dpartitionctx *dpart, bool matlabflag, int rank){

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

  strads_msg(ERR, "iohandler_partialread:rank(%d)  fn(%s), v_s(%ld) v_end(%ld) v_len(%ld) h_S(%ld) h_end(%ld) h_len(%ld)\n", 
	     rank, fn, dpart->range.v_s, dpart->range.v_end, dpart->range.v_len,  
	     dpart->range.h_s, dpart->range.h_end, dpart->range.h_len);

  // open file 
  if((rfd = open(dpart->fn, O_RDONLY)) == -1){
    strads_msg(ERR, "fatal: mach(%d) data partition file(%s) open fail\n", 
	       rank, dpart->fn);
    exit(-1);
  }

  readbytes = pbfio_read_bytes(rfd, 0, sizeof(pbfheader), (void *)fh);

  uint64_t fmaxrow = fh->maxrow;
  uint64_t fmaxcol = fh->maxcol;
  uint64_t frow_s = fh->row_s;
  uint64_t frow_end = fh->row_end;
  uint64_t fcol_s = fh->col_s;
  uint64_t fcol_end = fh->col_end;

  strads_msg(ERR, "@@@@@@@@@@@@@@@ frow_s(%ld) frow_end(%ld) fcol_s(%ld) fcol_end(%ld)\n", 
	     frow_s, frow_end, fcol_s, fcol_end);

  _range_sanity_check(frow_s, frow_end, fcol_s, fcol_end, 
		      dpart->range.v_s, dpart->range.v_end, 
		      dpart->range.h_s, dpart->range.h_end); 
  
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

#if 0 
  if(fh->nonzero != nzcnt){
    strads_msg(ERR, "Very WEIRD. Some thing wrong with data format header(%ld) read (%ld) \n",
	       fh->nonzero, nzcnt);	      
    exit(0);
  }
#endif 

  strads_msg(ERR, "\n\n\n in My local  binaryio.cpp: localnz: (%ld) all %ld\n\n\n\n", local_nzcnt, nzcnt);


  free(nzbuf);
  free(fh);

  close(rfd);
  return 0;
}




#if 0 
////////////////////////////////// REFERENCE CODE ONLY 
int main(void){

  int rfd, wfd;
  MD5_CTX c;
  uint8_t out[MD5_DIGEST_LENGTH];
  pbfheader *wheader = (pbfheader *)calloc(1, sizeof(pbfheader));

  MD5_Init(&c);

  srand(0x1202010);

  wfd = open("./testfile", O_WRONLY | O_CREAT, S_IRWXU);

  wheader->ptype=1;
  wheader->maxrow=0;
  wheader->maxcol=0;
  wheader->row_s=0;
  wheader->row_end=0;
  wheader->col_s=0;
  wheader->col_end=0;

  write_bytes(wfd, 0, sizeof(pbfheader), (void *)wheader);

  int64_t cpos = sizeof(pbfheader);
  nzentry *nzbuffer = (nzentry *)calloc(1024*1024, sizeof(nzentry));
  uint64_t nzcnt, tmpcol, totalcnt=0;
  uint64_t maxcol = 1024*1024*100;
  uint64_t maxrow = 1024*1024;

  nzcnt = 0;
  for(uint64_t row=0; row<maxrow; row++){    
    for(uint64_t col=0; col<400; col++){           
      tmpcol = rand()%maxcol;
      nzbuffer[nzcnt].row = row;
      nzbuffer[nzcnt].col = tmpcol;
      nzbuffer[nzcnt].val = (row+tmpcol)*1.15;
      nzcnt++;

      if(nzcnt == 1024*1024){
	write_bytes(wfd, cpos, nzcnt*sizeof(nzentry), (void *)nzbuffer);  
	cpos += nzcnt*sizeof(nzentry);
	totalcnt += nzcnt;
	MD5_Update(&c, (uint8_t *)nzbuffer, nzcnt*sizeof(nzentry));
	nzcnt=0;
      }
    }
  }

  write_bytes(wfd, cpos, nzcnt*sizeof(nzentry), (void *)nzbuffer);  
  cpos += nzcnt*sizeof(nzentry);
  totalcnt += nzcnt;
  printf("Write bytes: %d \n",  nzcnt*sizeof(nzentry));
  MD5_Update(&c, (uint8_t *)nzbuffer, nzcnt*sizeof(nzentry));

  MD5_Final(out, &c);
  printf("MD5 string for %ld totalcnt \n", totalcnt);
  for(uint64_t n=0; n < MD5_DIGEST_LENGTH; n++){
    printf("%02x", out[n]);
    wheader->md5key[n] = out[n]; 
  }
  printf("\n");
  write_bytes(wfd, 0, sizeof(pbfheader), (void *)wheader);  
  close(wfd);
  return 0;
}
#endif 
