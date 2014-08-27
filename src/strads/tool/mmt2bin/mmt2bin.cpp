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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

#include "mm_io.hpp"
#include "mmt2bin.hpp"

#define INPUTDELIM " \t	\n,   "
#define CONFIGDELIM " \t	\n,  ="

#define INF  1
#define DBG  2
#define ERR  3
#define OUT  4
#define MSGLEVEL ERR
#define strads_msg(level, fmt, args...) if(level >= MSGLEVEL)fprintf(stderr, fmt, ##args)


//#define MD5_VERIFY

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



long read_mmt_write_bin(char *rfn, char *wfn, bool matlabflag){

  long ret_code;
  MM_typecode matcode;
  FILE *f;
  uint64_t maxrow, maxcol, nzcnt, tmprow, tmpcol, cpos;
  double tmpval;

#if defined(MD5_VERIFY)
  MD5_CTX c;
  uint8_t out[MD5_DIGEST_LENGTH];
  MD5_Init(&c);
#endif 


  pbfheader *fh = (pbfheader *)calloc(1, sizeof(pbfheader));

  f = fopen(rfn, "r");
  assert(f);

  int wfd = open(wfn, O_WRONLY | O_CREAT, S_IRWXU);
  assert(wfd > 0);

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
  uint64_t entrycnt=1024*1024;
  uint64_t nzbufcnt=0;
  nzentry *nzbuf = (nzentry *)calloc(entrycnt, sizeof(nzentry));
  uint64_t accnz=0;

  cpos = sizeof(pbfheader);
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
    nzbuf[nzbufcnt].row = tmprow;
    nzbuf[nzbufcnt].col = tmpcol;
    nzbuf[nzbufcnt].val = tmpval;
    nzbufcnt++;
    accnz++;

    if(nzbufcnt == entrycnt){
      pbfio_write_bytes(wfd, cpos, nzbufcnt*sizeof(nzentry), (void *)nzbuf);

#if defined(MD5_VERIFY)
      MD5_Update(&c, (uint8_t *)nzbuf, nzbufcnt*sizeof(nzentry));
#endif

      cpos += nzbufcnt*sizeof(nzentry);
      nzbufcnt=0;
    }
  }

  assert(accnz == nzcnt);

  pbfio_write_bytes(wfd, cpos, nzbufcnt*sizeof(nzentry), (void *)nzbuf);

#if defined(MD5_VERIFY)
  MD5_Update(&c, (uint8_t *)nzbuf, nzbufcnt*sizeof(nzentry));
#endif 

  cpos += nzbufcnt*sizeof(nzentry);
  nzbufcnt=0;

  // fill the header  
  fh->maxrow = maxrow;
  fh->maxcol = maxcol;

  fh->row_s = 0;
  fh->row_end = maxrow-1;

  fh->col_s = 0;
  fh->col_end = maxcol-1;

  fh->nonzero = nzcnt;
  fh->physicalsort = 1;
  fh->origin = 1;
  fh->validhash = 1;

  printf("maxrow: %ld  maxcol: %ld  nz: %ld\n", 
	 fh->maxrow, fh->maxcol, fh->nonzero);
	   
#if defined(MD5_VERIFY)
  MD5_Final(out, &c);
  for(uint64_t n=0; n<MD5_DIGEST_LENGTH; n++){
    fh->md5key[n] = out[n];
  }
#endif

  // write the header at the beginning of the file 
  pbfio_write_bytes(wfd, 0, sizeof(pbfheader), fh);
  close(wfd);

  return 0;
}



int main(int argc, char **argv)
{
  if(argc != 3){
    printf("fatal: usage %s mmt-input-fn binary-output-fn\n", argv[0]);
    exit(-1);
  }
  read_mmt_write_bin(argv[1], argv[2], true); 
  return 0;
}
