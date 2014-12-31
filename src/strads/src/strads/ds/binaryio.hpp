
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

#include "common.hpp"
#include "ds/dshard.hpp"

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

long iohandler_spmat_pbfio_partialread(dshardctx *dpart, bool matlabflag, int rank);
long iohandler_spmat_pbfio_partialread(dshardctx *dpart, bool matlabflag, int rank, sharedctx *ctx);

long iohandler_spmat_pbfio_read_size(const char *fn, uint64_t *sample, uint64_t *feature, uint64_t *nz);


#if defined(COL_SORT_INPUT)
long iohandler_spmat_pbfio_partialread_sortedorder(dshardctx *dshard, bool matlabflag, int rank, sortingtype sortbase);
long iohandler_spmat_pbfio_partialread_sortedorder(dshardctx *dshard, bool matlabflag, int rank, sortingtype sortbase, sharedctx *ctx);
#endif 

#endif
