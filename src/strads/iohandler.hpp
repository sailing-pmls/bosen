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

#if !defined(IOHANDLER_H_)
#define IOHANDLER_H_

/* #define _GNU_SOURCE */ 
#include "basectx.hpp"
#include "dpartitioner.hpp"

/* strtok function to parse input data X, Y matrix */
#define INPUTDELIM " \t	\n,   "
#define CONFIGDELIM " \t	\n,  ="


long  iohandler_spmat_mmio_read(char *fn, dpartitionctx *dpart, dtype type);
long  iohandler_spmat_mmio_write(char *fn, dpartitionctx *dpart);
long iohandler_spmat_mmio_read_size(char *fn, uint64_t *sample, uint64_t *feature, uint64_t *nz);


void iohandler_sf_count(char *fn, long *samples, long *features);
void iohandler_allocmem_problemctx(problemctx *ctx, long samples, long features, long cmflag);
void get_datainfo(FILE *fd, long *samples, long *features);
void iohandler_read_fulldata(problemctx *pctx, int rank, int normalize, long unsigned int *randomcols);

void iohandler_makecm(problemctx *);
void iohandler_getsysconf(const char *, numactx *, problemctx *, int);
int  iohandler_diskio_loadsubmat(dpartitionctx *dpart, long v_s, long v_len, long h_s, long h_len);

void iohandler_sf_count_tmp(char *fn, uint64_t *samples, uint64_t *features);

// load one dense matrix without normalization.
//void iohandler_read_one_matrix(char *fn, double **X, uint64_t samples, uint64_t features);
void iohandler_read_one_matrix(char *fn, double **X, uint64_t samples, uint64_t features, uint64_t *randrow, uint64_t *randcol, bool randflag);


// If the origin of input data is (1, 1).. set matflag to 1 
long iohandler_spmat_mmio_partial_read(char *fn, dpartitionctx *dpart, bool matlabflag, int rank);

#endif 
