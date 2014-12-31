/********************************************************
  @author: Jin Kyu Kim (jinkyuk@cs.cmu.edu)
  part of STRADS - parallel scheduling framework for 
    model-parallel ML. 
********************************************************/
#if !defined(IOHANDLER_H_)
#define IOHANDLER_H_
/* #define _GNU_SOURCE */ 
#include <string>
#include <strads/ds/dshard.hpp>
#include <strads/ds/spmat.hpp>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
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

/* strtok function to parse input data X, Y matrix */
#define INPUTDELIM " \t	\n,   "
#define CONFIGDELIM " \t	\n,  ="

template<typename T>
long mmio_partial_read(int rank, T &mapmatrix, std::map<int,bool> &mybucket, std::string &fnstring){
  long ret_code;
  MM_typecode matcode;
  FILE *f;
  long unsigned int maxrow, maxcol, nzcnt, tmprow, tmpcol;   
  double tmpval;

  f = fopen((char *)fnstring.c_str(), "r");
  assert(f);
  if(mm_read_banner(f, &matcode) != 0){
    strads_msg(ERR, "mmio fatal error. Could not process Matrix Market banner.\n");
    exit(1);
  }
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
  strads_msg(ERR, "\t[worker %d] %ld nzcnt in header \n", rank, nzcnt);
  char *chbuffer = (char *)calloc(1024*1024*2, sizeof(char));
  uint64_t nzprogress=0;
  while(fgets(chbuffer, 1024*1024, f) != NULL){
    sscanf(chbuffer, "%lu %lu %lf", &tmprow, &tmpcol, &tmpval);   
    assert(tmprow < maxrow); // assume input file follow c origin starting from 0 
    assert(tmpcol < maxcol); // assume input file follow c origin starting from 0
    if(mapmatrix.m_type == strads_sysmsg::rm_map){
      if(mybucket.find(tmprow) != mybucket.end()){// my row 
	mapmatrix(tmprow, tmpcol) = tmpval;	
	nzprogress++;
      }// else: not belong to my rows. do nothing.  
    }else if(mapmatrix.m_type == strads_sysmsg::cm_map){
      if(mybucket.find(tmpcol) != mybucket.end()){// my row 
	mapmatrix(tmprow, tmpcol) = tmpval;	
	nzprogress++;
      }	//else :  not belong to my cols. do nothing.        
    }
    if(nzprogress %1000000 == 0 and nzprogress != 0)
      strads_msg(INF, "[worker %d] %ld nz\n", rank, nzprogress);
  }
  fclose(f);
  return nzprogress ;
}
#endif
