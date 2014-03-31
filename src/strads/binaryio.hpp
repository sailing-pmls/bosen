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
#if !defined(BINARYIO_HPP)
#define BINARYIO_HPP

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
#include "dpartitioner.hpp"
#include "basectx.hpp"

#define PBFIO_ENTRY_TO_READ (1024*1024)

typedef struct{
  uint8_t fn[256];

  void   *inmemp;  
  uint64_t partid;

  uint64_t maxrow;
  uint64_t maxcol;

  uint64_t row_s;
  uint64_t row_end;

  uint64_t col_s;
  uint64_t col_end;

  uint64_t nonzero;
  uint64_t valtype; // 1: 64bits double, 2: 64bit int, 3: 32bit float 4: 32bit int
  uint64_t idxtype; // 1: 64bit int 2: 32bit int  

  uint64_t signature;
  uint64_t physicalsort; // 1: row sort. 2: col sort  
  uint64_t origin; // 1: c ctyle starting at 0, 2: matlab starting at 1 

  uint64_t validhash; // 1: md5key is valid, 2: md5key is not valid.
  uint8_t  md5key[MD5_DIGEST_LENGTH];

}pbfheader;

typedef struct{
  uint64_t row;
  uint64_t col;
  double val;
}nzentry;

int pbfio_read_bytes(int rfd, int64_t offset,  size_t bytes, void *into);
void pbfio_write_bytes(int wfd, int64_t offset, size_t bytes, void *from);
long iohandler_spmat_pbfio_partial_read(char *fn, dpartitionctx *dpart, bool matlabflag, int rank);
long iohandler_spmat_pbfio_read_size(char *fn, uint64_t *sample, uint64_t *feature, uint64_t *nz);
long iohandler_spmat_pbfio_partial_read_wt_ooc(char *fn, dpartitionctx *dpart, bool matlabflag, int rank, param_range *selected_prange, int selected_prcnt);

#endif
