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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>

#include "systemmacro.hpp"
#include "utility.hpp"


using namespace std;


#define INPUTDELIM " \t	\n,   "

void lasso_read_y(uint64_t rank, char *yfn, uint64_t samples, double *vec){  

  char *ptr;
  FILE *fdy;
  char *linebuf;

  assert(yfn != NULL);
  fdy = fopen(yfn, "r");
  if(!fdy){
    strads_msg(ERR, "fatal: %s files open failed \n", yfn);
    exit(0);
  }

  linebuf = (char *)calloc(IOHANDLER_BUFLINE_MAX, sizeof(char));
  assert(linebuf);

  /* alloc Y vector and read Y file */
  for(uint64_t i=0; i<samples;i++){
    if(fgets(linebuf, IOHANDLER_BUFLINE_MAX, fdy) == NULL){
      printf("Input Matrix Y is not open linebuf[%s] i[%ld] out of samples:%ld\n", linebuf, i, samples);
      exit(0);
    }
    ptr = strtok(linebuf, INPUTDELIM);
    sscanf(ptr, "%lf", &vec[i]);
  }
  
  if(rank == 0)
    strads_msg(INF, "Y has [%ld] samples loading done\n", samples);

  free(linebuf);
  return;
}
