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
#include <map>
#include <strads/ds/mm_io.hpp>
#include <strads/sysprotobuf/strads.pb.hpp>
#include <strads/util/utility.hpp>

using namespace std;
using namespace strads_sysmsg;




#if 0 

#include "iohandler.hpp"
#include "systemmacro.hpp"
#include "utility.hpp"
#include "mm_io.hpp"
#include "dpartitioner.hpp"

using namespace std;
using namespace boost::numeric::ublas;

static void _standardizeInput(double **MX, double *MY, int n, int p);



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
#endif 
